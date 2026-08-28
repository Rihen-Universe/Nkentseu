# NKGui — l'introspection : demander ce qu'il y a à l'écran

> Ouvert le 2026-08-28. **Le critère d'acceptation (§2) a été écrit AVANT le
> code**, et il n'a pas été modifié en cours de route.
>
> Périmètre : **LIRE**. Pas modifier, pas agir. Ces deux chantiers-là viennent
> après, et rien ici ne prépare leur venue par des abstractions spéculatives.

---

## 0. Pourquoi — et le coût déjà payé, sans aucune IA

Trois pertes mesurées la même semaine, toutes de la même cause :

- trois étapes de NkUIDesign livrées sans que personne ne voie la fenêtre —
  c'est Rodolf qui a découvert, en lançant le binaire, qu'il n'y avait **rien**
  à l'écran ;
- sur NK3DModeler, un menu contextuel câblé, compilé, 25 raccourcis sans
  conflit — **invisible un tour entier** ;
- le sous-menu des six backends de NkUIDesign, **jamais photographié ouvert**,
  deux tours de suite.

Aucune de ces trois pertes n'est une panne de l'application. C'est **l'instrument
qui manque**. Le premier client de cet instrument est le **banc** et les agents
qui doivent prouver ce qu'ils livrent. Une IA pourrait s'en servir plus tard ;
ce n'est pas la raison, et l'instrument n'est pas dessiné pour elle.

---

## 1. Les trois mesures qui décident du dessin

Faites le 2026-08-28 sur `Nkentseu-noge`, avant toute conception.

### 1.1 NKGui est-il purement en mode immédiat ? — **OUI, pour la soumission**

`NkGuiContext::BeginFrame` remet à zéro, à chaque trame : `hotId`, `idDepth`,
`disabledDepth`, `curPopupLevel`, `winCount`, `containerDepth`, `overlayDepth`,
les deux listes de dessin. **Aucun arbre de contrôles n'est retenu.** Un widget
n'existe que pendant l'appel qui le soumet.

⚠️ **Mais le contexte retient de l'ÉTAT, indexé par identifiant** :
`openNodes`, `tabBarKeys`/`tabBarSel`, `scrollKeys`/`scrollVals`, `windowMeta`,
`dockNodes`, `popupStack`, `menuKeys`/`menuSizes`, `tblKeys`, `pickerKeys`.
Ce sont des **valeurs par id**, pas une hiérarchie : on ne peut pas les parcourir
pour reconstituer un écran, et ils ne disent rien d'un contrôle qui n'a pas
d'état persistant (un bouton, un libellé, une entrée de menu).

> **Conséquence de dessin** : l'introspection ne peut pas être l'interrogation
> d'un état persistant. C'est un **enregistrement pendant la trame** : chaque
> widget dépose sa note au moment où il se soumet, et la trame close, le relevé
> est lisible.

### 1.2 NKGui attribue-t-il des identifiants ? — **OUI**

`NkGuiId` = `uint32`, FNV-1a. `NkGuiContext::GetId(s)` hache la chaîne avec
pour graine le sommet de `idStack` (`PushId`/`PopId`, profondeur 32). Tout
widget interactif en dérive un : `ButtonEx`, `MenuItem`, `BeginMenu`,
`CheckboxCore`, `Selectable`… Et le contexte porte déjà `lastItemId` +
`lastItemRect`.

⚠️ **Trois limites, et elles bornent ce que le relevé peut promettre :**

1. **L'id est dérivé du LIBELLÉ.** Deux entrées de même texte dans deux menus
   différents ont le **même id** si la pile d'id est vide — et elle l'est dans
   la barre de menus de NkUIDesign. L'id désambiguïse *avec* le contexte de
   pile, pas tout seul.
2. **L'id est un hash 32 bits** : il ne se relit pas, il se compare. Le relevé
   le publie, mais ce qui NOMME un contrôle pour un lecteur humain reste le
   couple **(libellé, niveau)**.
3. **Rien ne garantit la stabilité au renommage** : changer un libellé change
   l'id. C'est acceptable — ce n'est pas une clé de persistance, c'est une clé
   de trame.

### 1.3 Que capture `UiRects` aujourd'hui ? — **DES RECTANGLES SEULS**

`Applications/NKUIDesign/src/NKUIDesign/Panels.h:139`. Structure :
`{ NkString id; NkRect r; }`. Le fichier produit est
`identifiant = x y w h`. **Rien d'autre.**

| ce que le banc demande | `UiRects` | conséquence |
|---|---|---|
| le rectangle | ✅ | — |
| le **libellé affiché** | ❌ | on ne distingue pas « autre chose que prévu » |
| **grisé** | ❌ | le grisage exigé par Rodolf est **improuvable** |
| **coché** | ❌ | la coche du backend courant est **improuvable** |
| la **nature** du contrôle | ❌ | un bouton et un titre se ressemblent |
| l'**imbrication** | ❌ | un sous-menu ne se distingue pas de son parent |

Et **trois défauts de conception** qui expliquent, à eux seuls, pourquoi le
sous-menu des backends n'a jamais été relevé :

1. 🔴 **L'inscription est MANUELLE, à chaque site d'appel.** Un contrôle que
   personne n'a pensé à instrumenter est **absent du relevé** — exactement
   comme s'il n'existait pas. Les six entrées du sous-menu n'ont jamais eu de
   `Note()` : voilà pourquoi elles ne sont jamais sorties.
2. 🔴 **Ce qui n'est pas visible n'est pas publié.** `UiRects::Note` refuse
   quand `ctx.layout.region.w < 4.f`. La garde a une bonne raison (une cible
   inatteignable ne doit pas passer pour prête, cf. carnet du 19/08) — mais
   elle rend **structurellement impossible** le critère « un contrôle câblé
   mais invisible doit se voir ». Le relevé confond « absent » et « invisible ».
3. 🔴 **Ça vit dans UNE application.** Deux fichiers, tous deux dans
   NKUIDesign. NK3DModeler, Nogee, NKCode, NKScena n'en héritent pas.

> **Le geste n'est pas de réécrire `UiRects` : c'est de le PROMOUVOIR** — même
> principe (lire ce qui a été *émis*, jamais des pixels), au bon étage, avec
> l'état en plus et sans le filtre qui aveugle.

### 1.4 Deux affirmations du mandat, vérifiées — et une est fausse

| affirmation | verdict |
|---|---|
| « NKGui n'a aucune API d'introspection » | ✅ **vrai** — 0 résultat sur `Introspect`, `QueryWidget`, `GetWidgetState`, `EnumerateWidgets` |
| « `--dump-ui` fonctionne déjà sans GPU » | ❌ **FAUX** |

`--dump-ui` ne fait que lever un drapeau ; le relevé est écrit par
`DumpUiRects`, posé en **overlay de la coquille** (`main.cpp:914`). Il exige
donc la **fenêtre entière, le GPU et la boucle de rendu**. Le seul chemin
réellement sans fenêtre est `--probe`, et `Probe.h` **ne construit aucun
`NkGuiContext`** : il n'a jamais vu un widget.

**Mais la propriété visée est atteignable, et c'est mieux que de la préserver :**
`NkGuiContext::Init` se réduit à `viewW = w; viewH = h; return true;`, NKGui
produit une liste de dessin et rien d'autre, et `NkGuiFont::LoadEmbedded`
construit son atlas **en mémoire vive** (l'envoi à la carte est le travail du
backend, pas de NKGui). **NKGui est entièrement calculable sans GPU** — la
propriété est donc à *créer*, pas à *garder*.

---

## 2. LE CRITÈRE D'ACCEPTATION

Écrit avant le code. Non révisable en cours de route.

### C1 — « rien » se distingue de « autre chose »

Un relevé d'écran vide doit être **lisible comme vide** (un compte de zéro,
explicite), et un relevé non vide doit porter assez d'information — nature,
libellé, état — pour qu'un lecteur qui attendait X et trouve Y **le voie sans
ouvrir une image**.

### C2 — un contrôle câblé mais invisible se voit

Un contrôle soumis dont le rectangle est **dégénéré** (largeur ou hauteur
nulle) ou **hors de la vue** est **relevé quand même**, et **marqué**. Il ne
disparaît pas du relevé.

> C'est la correction directe du défaut n°2 de `UiRects`. L'ancien filtre
> répondait à une question plus étroite que celle qu'on croyait poser.

### C3 — l'état sait dire « grisé » et « coché »

Sans quoi le grisage des menus, exigé par Rodolf sur les deux applications, est
improuvable. Ces deux états sont **obligatoires**, les autres sont un confort.

### C4 — l'état intermédiaire se rend, pas seulement le résultat

Un menu **ouvert** est un état intermédiaire : il n'a rien validé. Le relevé
doit le distinguer d'un menu fermé et d'un menu validé. Mesuré sur l'autre
chantier : *un compteur seul ne distingue pas « rien ne s'est passé » d'« un
aperçu attend confirmation »*.

### C5 — silencieux par défaut

Sans activation explicite, l'instrument ne fait **rien d'observable** : pas de
fichier, pas de journal, pas d'allocation par trame.

### C6 — sans fenêtre

Le relevé doit s'obtenir **sans GPU et sans fenêtre**. C'est cette propriété,
et elle seule, qui rend l'instrument utilisable par un agent.

### C7 — 🎯 LA PREUVE DE RECETTE

> Le sous-menu **Fichier ▸ Backend graphique ▸** de NkUIDesign, relevé
> **OUVERT**, avec ses **six** entrées (`auto`, `opengl`, `vulkan`, `dx11`,
> `dx12`, `software`) et **la coche sur celle que `nkuidesign.cfg` porte**.

Tant que l'instrument ne sait pas produire ça, il n'est pas fini. Ce cas a
résisté deux tours à l'agent précédent.

### Ce que le critère n'exige PAS

- pas de couverture des 114 fonctions de `NkGuiWidgets.h` ;
- pas d'API pour **modifier** ni pour **agir** ;
- pas de format binaire, pas de schéma versionné, pas de sérialisation
  générique. Le texte suffit, et il se `grep`.

---

## 3. Le dessin

### 3.1 Où

**Dans NKGui** — `Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiIntrospect.{h,cpp}`.
C'est la couche qui dessine, donc c'est elle qui sait ce qu'elle a dessiné.
NKEditorKit et **toutes** les applications en héritent sans une ligne.

### 3.2 Un enregistrement, pas une requête

Chaque widget instrumenté appelle `NkGuiNoter(...)` au moment où il se soumet.
Le contexte accumule les notes de la trame ; `BeginFrame` les vide. La lecture
se fait **après `EndFrame`**.

### 3.3 Ce qu'une note porte

`id` · `nature` · `états` (masque) · `niveau` d'imbrication · `rect` ·
`libellé` · `annexe` (raccourci, valeur). Tailles fixes, zéro allocation par
note après la première trame — NKGui est zéro-STL et le reste.

### 3.4 Silence

Deux voies d'activation, et aucune par défaut : `NkGuiIntrospectActiver(ctx,
true)` depuis le code, ou la variable d'environnement `NK_GUI_INTROSPECT=1`
lue une fois à `Init` — même modèle que `NK_MENU_TRACE=1` sur l'autre chantier.

⚠️ **`NK_MENU_TRACE` n'existe pas dans cet arbre** (0 résultat, contrôle
positif fait sur `NK_GUI_INTROSPECT` après écriture). Il vit sur le chantier
NK3DModeler. C'est un modèle cité, pas une dépendance.

### 3.5 Ce que l'instrument NE fait PAS, et pourquoi c'est écrit ici

Le banc qui pilote la souris écrit dans `ctx.input` — le **même** champ public
que toute application remplit depuis ses événements OS. **Aucune fonction
d'injection n'est ajoutée à NKGui.** La frontière « lire » / « agir » est
gardée : quand le chantier « agir » s'ouvrira, il partira d'une page blanche,
pas d'une demi-abstraction posée par avance.

### 3.6 Une objection antérieure, et pourquoi elle ne tient plus

`Panels.h:122` a écarté, en son temps, « élargir NKGui pour exposer un
identifiant de test → refusé : élargir un module partagé pour un besoin d'essai
est une dette qu'on rembourse pendant des mois. »

Le refus était juste **pour ce qu'il refusait** : un crochet d'essai. Ce qui
est écrit ici n'est pas un crochet d'essai — c'est une **capacité du socle**,
justifiée par trois pertes mesurées **hors de tout essai**, et dont les
consommateurs sont les quatre éditeurs, pas une famille de sondes. La porte du
dépôt tranche dans l'autre sens : *quand la couche du dessous ne porte pas la
chose, on la fait grossir là, pas chez soi.*

---

## 4. La recette, prouvee — 2026-08-29

### 4.1 Ce qui a ete ecrit, et ou

| fichier | lignes | role |
|---|---|---|
| `Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiIntrospect.h` | ~185 | les types, les etats, l'API |
| `Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiIntrospect.cpp` | ~215 | le depot, la recherche, le rendu texte |
| `NkGuiContext.h` / `.cpp` | +27 | le releve dans le contexte, vide au `BeginFrame`, `NK_GUI_INTROSPECT` lu a `Init` |
| `NkGuiWidgets.cpp` | +60 | dix widgets instrumentes |
| `Applications/NKUIDesign/.../main.cpp` | +210 | `--releve-menus` : le harnais sans fenetre |

**Le diff est PUREMENT ADDITIF** : 0 ligne supprimee sur les cinq fichiers
existants. Rien de ce qui marchait n'a change de comportement.

Dix widgets notent : `ButtonEx`, `Text`, `CheckboxCore`, `Selectable`,
`Separator`, `BeginMenuBar`, `BeginMenu`, `MenuItem`, `BeginPanel`, plus la
barre elle-meme. C'est deliberement peu — de quoi tenir le critere, pas les 114
fonctions de `NkGuiWidgets.h`. **Ce qui manque s'ajoute en trois lignes par
widget**, au moment ou un chantier en aura besoin.

### 4.2 🎯 C7 — le sous-menu des backends, releve OUVERT

Sans fenetre, sans GPU, sans carte graphique sollicitee :

```
$ ./Build/Bin/Debug-Windows/NKUIDesign/NKUIDesign.exe --releve-menus
[NKUIDesign/releve] nkuidesign.cfg porte gfx='vulkan'
[NKUIDesign/releve] 34 controle(s) releve(s)
[NKUIDesign/releve] sous-menu 'Backend graphique' : OUVERT - 6/6 entrees, 1 cochee(s).
[NKUIDesign/releve] RECETTE PROUVEE.
$ echo $?
0
```

Le releve complet, tel qu'il sort (`nkuidesign.cfg` portant `gfx=vulkan`) :

```
# NKGui introspection : 34 controle(s), 0 hors-vue, 0 degenere(s), 0 perdue(s)
# vue 1456x939
# nature niveau rect=[x y w h] etats id "libelle" [annexe]
barre-menus n-1  [   56.0     0.0  1274.0    28.0] normal                   0xff530e1c ""
menu        n-1  [   60.0     0.0    56.2    28.0] ouvert                   0xe6db0839 "Fichier"
entree      n0   [   70.0    38.0   204.9    26.0] normal                   0x9055660c "Nouveau projet…" [Ctrl+N]
entree      n0   [   70.0    70.0   204.9    26.0] grise                    0xc7601e62 "Ouvrir…" [Ctrl+O]
menu        n0   [   70.0   102.0   204.9    26.0] replie                   0x4e8e3b82 "Ouvrir récent"
entree      n0   [   70.0   134.0   204.9    26.0] grise                    0x78558ef5 "Fermer le projet" [Ctrl+W]
separateur  n0   [   70.0   166.0   204.9     1.0] normal                   0x00000000 ""
entree      n0   [   70.0   173.0   204.9    26.0] normal                   0x68686cf5 "Enregistrer" [Ctrl+S]
entree      n0   [   70.0   205.0   204.9    26.0] grise                    0x32e02903 "Enregistrer sous…" [Ctrl+Maj+S]
entree      n0   [   70.0   237.0   204.9    26.0] grise                    0x1d8d585b "Enregistrer tout" [Ctrl+Alt+S]
entree      n0   [   70.0   269.0   204.9    26.0] normal                   0x60c37d9c "Revenir à la version enregistrée"
separateur  n0   [   70.0   301.0   204.9     1.0] normal                   0x00000000 ""
menu        n0   [   70.0   308.0   204.9    26.0] replie                   0xe29d5323 "Importer"
menu        n0   [   70.0   340.0   204.9    26.0] replie                   0xa746a4d0 "Exporter"
entree      n0   [   70.0   372.0   204.9    26.0] grise                    0x673f108e "Valider le document" [Ctrl+Maj+V]
separateur  n0   [   70.0   404.0   204.9     1.0] normal                   0x00000000 ""
menu        n0   [   70.0   411.0   204.9    26.0] survole,ouvert           0x8d340a03 "Backend graphique"
entree      n1   [  293.9   420.0    66.0    26.0] normal                   0x923fa396 "auto"
entree      n1   [  293.9   452.0    66.0    26.0] normal                   0xd4f4f9f2 "opengl"
entree      n1   [  293.9   484.0    66.0    26.0] coche                    0xfc99b1f2 "vulkan"
entree      n1   [  293.9   516.0    66.0    26.0] normal                   0xa2c954c3 "dx11"
entree      n1   [  293.9   548.0    66.0    26.0] normal                   0xa3c95656 "dx12"
entree      n1   [  293.9   580.0    66.0    26.0] normal                   0x42278e90 "software"
entree      n0   [   70.0   443.0   204.9    26.0] grise                    0xf0192adf "Préférences…" [Ctrl+,]
separateur  n0   [   70.0   475.0   204.9     1.0] normal                   0x00000000 ""
entree      n0   [   70.0   482.0   204.9    26.0] normal                   0xcbc1d0b1 "Quitter" [Ctrl+Q]
menu        n-1  [  116.2     0.0    56.6    28.0] replie                   0x42998a3c "Édition"
menu        n-1  [  172.8     0.0    72.0    28.0] replie                   0x925a0913 "Affichage"
menu        n-1  [  244.8     0.0    48.9    28.0] replie                   0x71d908db "Objet"
menu        n-1  [  293.7     0.0    47.4    28.0] replie                   0xc6d3853e "Cible"
menu        n-1  [  341.1     0.0   100.8    28.0] replie                   0x860d6718 "Comportement"
menu        n-1  [  441.9     0.0    30.9    28.0] replie                   0x5be8f33f "IA"
menu        n-1  [  472.8     0.0    61.3    28.0] replie                   0xe7c7f3ec "Fenêtre"
menu        n-1  [  534.1     0.0    44.2    28.0] replie                   0x6679d72c "Aide"
```

**Ce que ce texte prouve, ligne par ligne :**

- `menu n-1 … ouvert "Fichier"` — le menu de la barre est **deroule** (C4) ;
- `menu n0 … survole,ouvert "Backend graphique"` — le sous-menu est **ouvert**,
  et il est **au niveau 0**, dans le popup de « Fichier » (C7) ;
- **six** `entree n1` — `auto`, `opengl`, `vulkan`, `dx11`, `dx12`, `software`,
  au niveau 1, donc dans le flyout (C7) ;
- `coche` sur **`vulkan`**, et sur elle seule — ce que `nkuidesign.cfg` porte (C3, C7) ;
- `grise` sur six entrees de « Fichier » — le grisage exige par Rodolf devient
  **prouvable** pour la premiere fois (C3) ;
- neuf `menu n-1` — les neuf entrees de barre de la decision du 2026-08-20.

### 4.3 Les contre-epreuves — un banc qui ne sait dire que « oui » ne mesure rien

Deux mutations temporaires, posees puis **retirees** (residu verifie a 0).

| mutation | attendu | mesure |
|---|---|---|
| **A** — `vulkan` retire de la boucle du menu | le harnais REFUSE | `5/6 entrees`, `RECETTE NON PROUVEE`, **code de sortie 1** ✅ |
| **B** — `SetNextItemRect({0,0,0,0})` avant `dx12` | l'entree est relevee **et marquee** | `entree n1 [0.0 0.0 0.0 0.0] hors-vue,vide "dx12"` ✅ |

**La mutation B est la demonstration du critere C2**, et c'est celle qui compte
le plus : `dx12` est **cablee, compilee, soumise a NKGui — et invisible**.
`UiRects` n'aurait rien montre du tout (sa garde `region.w < 4` la refusait), et
le releve aurait fait croire a une entree absente. Ici elle est **la**, marquee
`hors-vue,vide`. C'est exactement le cas qui a coute un tour entier a
NK3DModeler et trois etapes a NkUIDesign.

### 4.4 Le releve est-il stable ?

Sept executions appariees, meme binaire, meme configuration :

```
run1..run7 : code=0, sha256 identique (fdf4a033c19c9ded…) — 7/7
```

**Dispersion nulle.** C'etait la question a poser : un instrument qui rend une
reponse differente a chaque tir ne sert a rien, et le depot a deja mesure 93 %
de dispersion sur une execution unique ailleurs. Ici l'absence de fenetre et de
GPU y est pour beaucoup — il n'y a ni pilote, ni vsync, ni ordonnancement a
subir.

### 4.5 Ce qui n'etait PAS fait au soir du 2026-08-28

*(Trois de ces cinq points ont ete traites le 29 — voir le §5. La liste reste
ici telle qu'ecrite : biffer ce qu'on a promis fait mal l'histoire.)*

- ❌ **`UiRects` n'est pas retire de NkUIDesign.** Les deux coexistent. C'est le
  piege deja nomme pour la capture d'ecran : *remonter la capacite ne supprime
  pas les copies ; tant qu'elles vivent, elles divergeront.* **Le chantier ne
  sera fini que quand NkUIDesign appellera le releve de NKGui et que `UiRects`
  aura disparu.** Ce n'est pas fait, et c'est le premier travail suivant.
- ❌ **104 des 114 widgets ne notent rien.** Un ecran d'editeur complet ne se
  releve donc pas encore en entier.
- ❌ **Aucun banc ne lance `--releve-menus` automatiquement.** Il faut l'appeler
  a la main ; rien ne le rougira si quelqu'un casse le menu.
- ❌ **Le releve n'est pas branche sur l'application FENETREE.** `NK_GUI_INTROSPECT=1`
  remplit bien le releve pendant la vraie boucle, mais **personne n'appelle
  `NkGuiIntrospectEcrire`** dans ce chemin : il faudrait une ligne dans l'overlay
  de la coquille, a cote de `DumpUiRects`. Non fait a dessein — la borne du
  chantier etait la preuve du sous-menu.
- ❌ **`--releve-menus` ne releve QUE la barre de menus.** Il n'instancie ni la
  coquille, ni les panneaux, ni le dock. Un « releve de l'ecran entier sans
  fenetre » est un autre travail, plus gros, et il commence par savoir monter
  `NkEditorShell` sans renderer.
- ⚠️ **Deux etats sont declares et jamais poses par aucun widget** :
  `NK_GUI_ETAT_ATTENTE` et `NK_GUI_ETAT_FOCALISE`. Ils existent parce que le
  critere C4 les nomme, mais **aucune ligne ne les emet aujourd'hui** — c'est
  une promesse au lecteur du fichier, pas une capacite. A poser par le chantier
  qui en aura l'usage, ou a retirer.

---

## 5. 2026-08-29 — `UiRects` est mort, et le chemin fenetre est branche

### 5.1 Pourquoi ca ne pouvait pas attendre

> « Les deux coexistent, donc ils divergeront. »

C'etait ecrit au §4.5 le soir meme, et c'est exactement comme ca qu'un doublon
s'installe pour un an. **Le but n'a jamais ete que les deux rendent la meme
chose : c'est qu'il n'y en ait plus qu'un.** Un doublon qui s'accorde
aujourd'hui est un doublon qui divergera demain — ce depot l'a paye cette
semaine avec deux registres de roles homonymes et deux objets theme.

### 5.2 Ce qui a ete mesure AVANT de renommer le fichier de sortie

Le format du releve change entierement (nature, niveau, etats, cle, libelle au
lieu de `identifiant = x y w h`). **Garder le nom `nkuidesign_ui_rects.txt`
aurait laisse un script lire un format qu'il ne comprend plus, sans rien casser
de visible.** Mesure sur tout l'arbre avant de trancher :

```
nkuidesign_ui_rects.txt  ->  .gitignore, le carnet, et le site d'ecriture.
                             AUCUN lecteur.
```

Le renommage etait donc **gratuit** — mais il ne l'aurait pas ete, et c'est la
verification qui le dit, pas l'intuition. Nouveau nom :
`nkuidesign_releve_ui.txt`.

### 5.3 Ce que NKGui a du apprendre pour absorber `UiRects`

`UiRects` faisait trois choses que le releve ne savait pas faire. Elles sont
entrees dans le socle, additivement :

| besoin de l'application | ce qui a ete ajoute |
|---|---|
| publier une **zone calculee** (region d'un panneau hote, aire de dessin) qui n'est le rectangle d'aucun widget | `NkGuiNature::Region` |
| publier **quatre nombres qui ne sont pas une geometrie** (`canvas.vue` portait zoom / panX / panY / selection dans un `NkRect`) | `NkGuiNature::Mesure` + `NkGuiNoterMesure` |
| donner au widget une **cle stable** qui ne bouge pas quand le libelle change | champ `cle` + `NkGuiIntrospectCler` |

🔴 **`Mesure` n'est pas un confort, c'est une garde.** Sans elle, le releve
aurait juge ces quatre nombres comme une geometrie : un zoom de 1 et un
deplacement nul seraient sortis `vide,hors-vue`. **Une fausse alerte dans un
instrument coute plus cher que pas d'alerte du tout — elle apprend a ne plus le
lire.**

🔴 **`NkGuiIntrospectCler` NE CREE PAS DE NOTE**, et c'est ce qui distingue une
promotion d'une recopie. L'ancien `UiRects::Note` ajoutait *sa* ligne a cote du
widget ; ici il n'y a **qu'une note, avec deux noms** — le libelle affiche et la
cle stable. Deux lignes pour un bouton, c'etaient deux verites a maintenir.

### 5.4 Ce qui reste dans l'application, et pourquoi ce n'est pas un doublon

Trois adaptateurs **sans etat** dans `Panels.h` : `releve::Zone`,
`releve::Rect`, `releve::Cle`. Ils ne stockent rien, ne filtrent rien,
n'ecrivent rien. **Ils nomment** — et le nommage appartient bien a
l'application : NKGui ne peut pas savoir que ce panneau s'appelle
« hierarchie ». Le registre (stockage, filtre, ecriture) a entierement quitte
l'application.

⚠️ **Le filtre, lui, n'a pas ete deplace : il a ete SUPPRIME.** `UiRects::Note`
refusait de publier quand `region.w < 4`. La garde avait raison contre ce
qu'elle visait — une cible inatteignable ne doit pas passer pour prete — mais
elle confondait **absent** et **invisible**. NKGui note toujours et marque : un
panneau replie sort `vide`, un panneau hors champ sort `hors-vue`, un panneau
jamais dessine ne sort pas. **Les trois cas se distinguent enfin.**

### 5.5 Le chemin FENETRE, branche

`--dump-ui` ne leve plus un booleen global : il memorise l'intention, et
l'instrument s'allume **a la creation de la coquille**, pas dans l'overlay.
Active depuis l'overlay, il aurait rate la **premiere image entiere** — sans
importance pour un editeur qui tourne, decisif pour une capture prise au
demarrage, qui est l'usage vise.

Mesure, application fenetree, `--dump-ui --small` :

```
# NKGui introspection : 47 controle(s), 0 hors-vue, 0 degenere(s), 0 perdue(s)
# vue 1040x679

  17 bouton      10 region     9 menu      5 texte
   3 panneau      1 separateur 1 mesure    1 barre-menus
```

**`UiRects` en publiait 0 sur les 17 boutons et les 5 libelles** : il ne voyait
que ce que quelqu'un avait pense a instrumenter a la main.

⚠️ **Un defaut vu en passant, et qui n'est pas de ce chantier** : les trois
panneaux sortent avec une hauteur de **1 000 000**. `ctx.layout.region.h` n'est
pas borne pour un panneau ancre. Ce n'est pas faux au sens du releve — c'est ce
que l'hote pose reellement — mais aucun banc ne pourra viser le bas d'un
panneau tant que ca dure. **Porte au canal, ce n'est pas un fichier de NKGui.**

### 5.6 La contre-epreuve de la migration

Une suppression se prouve autrement qu'un ajout : il faut montrer que ce qui
sort vient bien du **nouveau** chemin, et pas d'un reste de l'ancien.

| mutation C | mesure |
|---|---|
| `releve::Zone(ctx, "hierarchie")` retire | `panneau.hierarchie` **disparait**, et **lui seul** — `apercu` et `inspecteur` restent |

Mutation retiree, residu verifie a 0, vert revenu, recette `--releve-menus`
toujours a 0.

**Et l'inventaire des cles d'avant** (relu dans le source d'avant la migration,
pas de memoire) : `apercu`, `canvas.vue`, `compo.section_document`,
`composition`, `hierarchie`, `inspecteur`, `palette`, `preferences`, plus les
cles dynamiques `apercu.nœud.*`, l'arbre (`pages`, `composants`) et les cellules
`NoteCell`. Toutes celles qui sont **atteignables dans cette configuration** se
retrouvent dans le nouveau releve. Les autres —  `composition`, `preferences`,
`compo.section_document`, `prefs.gfx.*` — sont derriere
`#define NKUIDESIGN_ANCIENS_PANNEAUX 0`, et `palette` est enregistree fermee
(`palette.SetOpen(false)`) : **elles etaient deja inatteignables avant la
migration**. Aucune perte.

### 5.7 Les deux etats fantomes : RETIRES

`NK_GUI_ETAT_ATTENTE` et `NK_GUI_ETAT_FOCALISE` etaient dans l'enumeration et
**aucune ligne ne les emettait**. Ils sont supprimes.

> Un etat qui existe dans le type et jamais dans les faits est **pire qu'un
> manque** : le lecteur voit l'absence du mot et conclut « ce controle n'attend
> rien », alors que la verite est « personne n'a jamais mesure ca ». C'est la
> famille du zero d'un champ mort, qu'on ne distingue pas du zero d'un champ
> vivant.

**Le critere C4 reste tenu** — par `NK_GUI_ETAT_OUVERT`, qui, lui, est pose par
`BeginMenu` et prouve par la recette. Les remettre demandera un producteur
reel : `ctx.inputId` pour le focus d'un champ texte, et une notion d'apercu que
NKGui n'a pas.

### 5.8 Ce qui reste ouvert

- **Les 104 widgets non instrumentes restent non instrumentes**, volontairement.
  Trois lignes chacun le jour ou un chantier en a besoin ; une note posee sans
  usage est du code non exerce.
- **Aucun banc ne lance `--releve-menus`.** Ou ca vit est mesure : le
  verificateur du depot est dans l'arbre **`Nkentseu-verif`** (branche
  `feat/verificateur`) — `verif_bancs.sh` + `config/bancs.list`. **NKUIDesign y
  a deja sa ligne** (ligne 72), classee `banc`, mode `rapide`, arguments
  `--roundtrip-controles`, verdict `code`. ⚠️ **Le format est UN PROJET PAR
  LIGNE** : `--releve-menus` ne s'ajoute donc pas a cote, il faudrait soit
  remplacer les arguments, soit apprendre au verificateur a lancer plusieurs
  invocations pour un meme projet. **Ce n'est ni mon arbre ni mon fichier** —
  la question est posee, pas tranchee.
