# NKGui — Roadmap

État : **Phase 1 — squelette** (voir `README.md` et `ARCHITECTURE.md`). Réécriture
destinée à remplacer NKUI, deux paradigmes (immédiat et retenu).

---

# MANDAT — Le socle d'interaction qui manque (2026-08-04)

> Rédigé depuis NK3DModeler, à la demande de Rihen, pour un agent qui n'y
> travaille pas. **Objet** : trois défauts structurels d'interface, rencontrés et
> corrigés à la main dans NK3DModeler, qui se reposeront dans toute application
> tant que le socle ne les traite pas. À vérifier **aussi dans NKCode**.
>
> NKGui étant encore un squelette, ces leçons ont vocation à entrer dans sa
> **conception**, pas à être rattrapées après coup.

## L'état des lieux, sans complaisance

Le socle existe déjà — en double, et les applications l'ignorent.

| Couche | Ce qu'elle porte | Qui s'en sert |
|---|---|---|
| **NKGui** | `NkGuiContext::NkInputLayerScope` (**priorité par couche**), `TextWrapped`, `Button`, `Checkbox`, `NkGuiDrawList`, `NkGuiInput` | **NKCode** |
| **NKEditorKit** (`Engine/`) | `NkEditorCombo`, `NkEditorContextMenu`, `NkEditorModal`, `NkEditorPanel`, `NkEditorTooltip`, `NkEditorTextField`, `NkEditorScrollbar` | partiellement |
| **NK3DModeler** | `NkModelerWidgets.h` (Combo, DragFloat, EditableText), `NkModelerUI.h` (painter), `NkHitRegistry` (**LayerScope réécrit**), `TextWrap` (**réécrit**) | lui seul |

**Le constat, vérifié le 4 août 2026 :**

- **NKCode utilise NKGui** — `NkGuiContext::NkInputLayerScope(ctx, 50)` dans son
  panneau IA, `(ctx, 100)` dans ses commandes. Il consomme le socle.
- **NK3DModeler a tout réécrit** — `NkHitRegistry::LayerScope`, son propre
  painter, ses propres widgets. Il ne consomme rien.

Les deux briques que le modeleur a réécrites **existaient déjà dans NKGui** :
la priorité par couche et le texte qui va à la ligne. C'est donc bien un
**contournement**, pas une lacune du socle.

Nuance à ne pas perdre : la version NKGui n'a peut-être pas tout ce dont le
modeleur a besoin (le report d'une image pour les surcouches peintes *après* le
panneau qu'elles couvrent, notamment — voir défaut 1). **La première tâche est
donc de comparer les deux implémentations**, pas de supposer que l'une remplace
l'autre. NKCode, qui vit déjà sur NKGui, est le bon témoin : s'il ne souffre pas
des quatre bugs listés plus bas, la version NKGui suffit et le modeleur n'a qu'à
migrer. S'il en souffre en silence, le socle est à compléter.

## Défaut 1 — La priorité d'interaction n'est pas dérivée de l'ordre de peinture

**Le plus coûteux des trois.** Aujourd'hui, trois mécanismes complémentaires
doivent être câblés **à la main dans chaque panneau** :

- `NkHitRegistry::LayerScope(hit, 50)` — monter les surcouches d'une couche ;
- `hit.SetBlock(rect, on)` — ignorer les clics d'une emprise ;
- `st.UiBlockAdd(rect)` / `st.UiBlocks(mx, my)` — emprise mémorisée d'une image
  sur l'autre, pour les surcouches peintes *après* le panneau qu'elles couvrent.

Quatre bugs le 4 août 2026, tous de la même famille :

1. le menu d'en-tête de groupe peint sur la couche du panneau : clics inopérants
   et menu qui ne se refermait pas ;
2. le panneau de l'édition proportionnelle laissait passer ses clics ;
3. la liste déroulante du format de sortie : « je ne peux choisir aucun format » —
   le panneau Propriétés ne consultait pas `UiBlocks`, alors qu'il porte le plus
   de listes de toute l'application ;
4. **ma propre correction du point 3** : en armant le blocage, j'ai neutralisé la
   liste elle-même, peinte après. D'où la règle qui manquait, à inscrire dans le
   socle : **le blocage protège ce qui est dessous, jamais ce qui est au-dessus.**

**Directive.** Dans un socle correct, l'ordre de peinture définit la priorité
d'interaction, sans rien à armer. Ce qui est peint plus tard capte le clic en
premier ; le reste en découle. Si le paradigme immédiat impose de connaître les
emprises avant de les peindre, alors le report d'une image doit être **interne au
socle** — jamais une responsabilité de l'appelant.

## Défaut 2 — Les surcouches différées gardent des pointeurs vers la pile

`NkComboPending` (NK3DModeler) conserve `const char *const *items` **et**
`int32 *selected` pour peindre la liste plus tard, hors de la portée qui l'a
créée. Deux bugs distincts en sont sortis le même jour :

1. **Pointeurs morts** — un tableau de libellés ou une variable de sélection
   *locale* est détruit avant que la liste ne soit peinte. Symptôme : le combo
   s'ouvre, choisir ne change rien (au mieux) ou l'application plante (au pire —
   le code du modeleur documente déjà ce second cas pour les items).
2. **Deux sources concurrentes** — comparer la sélection mémorisée à la vérité du
   moteur ne suffit pas : quand c'est le *moteur* qui a changé la valeur, l'écart
   se lit comme un choix de l'utilisateur et l'ancienne valeur est réappliquée.
   Il faut mémoriser la dernière valeur **vue** du moteur, et lui donner la
   priorité. Symptôme observé : un échange principale/miniature s'appliquait puis
   s'annulait à l'image suivante.

**Directive.** Une surcouche différée ne doit **jamais** détenir de pointeur vers
la pile de l'appelant. Deux voies acceptables : copier les libellés et la valeur
dans le socle, ou fonctionner par **identifiant** (le socle rend le choix,
l'appelant l'interroge). La seconde est préférable en mode immédiat.

Corollaire, à documenter dans le contrat : quand une valeur peut changer des deux
côtés (interface et modèle), le socle doit rendre explicite **qui a la priorité**.
Un simple `if (a != b)` ne peut pas le savoir.

## Défaut 3 — Le texte n'est pas contraint par son conteneur

Le système de groupes de NK3DModeler cadre les **widgets** — leurs rectangles se
calculent depuis la largeur du groupe — mais pas les **chaînes**, qui se peignent
où on leur dit. Un libellé plus large que la colonne débordait tel quel.

Ce n'était pas un défaut du système de groupes : c'était une brique manquante.
`TextWrap` (retenu : coupe aux espaces, place seul un mot trop large, renvoie la
**hauteur consommée** pour que l'appelant avance son curseur sans compter les
lignes) a été ajouté au painter du modeleur — alors que **`NKGui::TextWrapped`
existait déjà**.

**Directive.** Vérifier que `TextWrapped` couvre ces cas, et que le socle expose
une notion de **conteneur** dont le texte hérite — pas seulement un `wrapWidth`
que chaque appelant doit calculer.

## Ce qui ne relève PAS de ce mandat

À dire explicitement, pour ne pas élargir le périmètre : plusieurs bugs de la même
session **ne sont pas** des défauts d'interface, et aucun socle UI ne les aurait
évités.

- `NkImage::Free()` libère l'objet (`nkFree(this)`) et non ses pixels : appelée
  sur un objet statique, elle fermait l'application. `Unload()` est la méthode qui
  vide. **Nommage**, pas interface.
- Un identifiant de **nœud** passé à une fonction qui attend un index d'**objet** :
  deux espaces d'indices, aucune erreur de compilation, un nom pris au hasard
  (« mur gi » pour une caméra). **Typage métier.**
- Une table de libellés indexée par entier restée à 6 entrées quand une 7ᵉ section
  est apparue : l'en-tête n'affichait plus de nom. **Une table unique décrivant
  les sections** l'aurait évité — pas un socle UI.

## Ordre proposé

1. **Comparer `NkGuiContext::NkInputLayerScope` (NKGui, utilisé par NKCode) et
   `NkHitRegistry::LayerScope` (réécrit par NK3DModeler).** Le second gère en
   plus `SetBlock` et le report d'une image (`UiBlockAdd`/`UiBlocks`) : vérifier
   si le premier en a l'équivalent, et **tester NKCode sur les quatre cas du
   défaut 1** — un panneau dont une liste déroulante recouvre d'autres widgets
   cliquables. S'il y résiste, le modeleur n'a qu'à migrer.
2. **Défaut 1 dans NKGui** — priorité dérivée de l'ordre de peinture. Le plus
   structurant, et celui qui doit être décidé avant que l'API se fige.
3. **Défaut 2** — contrat des surcouches différées, sans pointeur vers la pile.
4. **Défaut 3** — vérifier `TextWrapped`, exposer la notion de conteneur.
5. **Migration progressive**, jamais en bloc : NK3DModeler et NKCode fonctionnent.
   Faire remonter une brique à la fois, en commençant par celle qui coûte le plus.

## Pour l'agent qui reprend

Les cas réels sont consignés, avec leur cause et leur correction, dans
`CARNET.private.md` à la racine (vagues 40 à 42) et dans
`Applications/NK3DModeler/ROADMAP.md`. Ils valent mieux qu'une description
abstraite : chacun a un symptôme observable et une cause identifiée.

Contrainte de travail du dépôt : **zéro STL**, français dans les commentaires,
build Debug **et** Release vérifiés à 28/28.
