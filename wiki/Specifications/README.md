# Les spécifications — où elles vivent, et pourquoi pas ici

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**26/09/2026.**

> 🔴 **Les documents ne sont PAS dans ce dossier.** Ils vivent **à côté de leur
> application**, dans `Applications/<App>/design/`. Ce fichier n'est qu'un
> panneau indicateur.

---

## Où chercher

| application | ce qu'elle est | ses documents |
|---|---|---|
| **NKCraft** | le modeleur 3D | `Applications/NKCraft/design/` |
| **NkAnimaEditor** | l'atelier d'animation | `Applications/NkAnimaEditor/design/` |
| **Nogee** | l'éditeur de jeu, sur Noge | `Applications/Nogee/design/` |
| **NKScena** | la mise en scène | `Applications/NKScena/design/` |
| **PV3DE** | le médical | `Applications/PV3DE/design/` |

Chaque dossier porte **trois documents** :

1. **le produit** — *la cible réelle*. Rodolf, le 26/09 : « le produit est ce que
   je veux réellement. » Les deux autres le servent.
2. **l'interface, pour un humain** — l'UX, le style, les espaces de travail.
3. **l'interface, pour l'agent** — l'arbre des éléments nommés, ce qui se répète
   (les composants), ce qui agit (les actions), ce qui est de la couleur **et
   pourquoi**, plus ce que la chaîne ne sait pas encore reproduire, **écrit
   quand même**.

---

## ⚠️ POURQUOI CE DOSSIER EST VIDE — une erreur payée le jour même

Le 26/09 à 19h47, ces neuf documents ont été copiés **ici**, dans
`wiki/Specifications/`. Vingt-six minutes plus tard, une archive rangée en
apportait quinze — cinq applications au lieu de trois — et **les documents
communs avaient changé** :

| document | écart avec la copie de 19h47 |
|---|---|
| `01-produit-nkscena` | 10 lignes |
| `07-produit-nkanima` | 8 lignes |
| `09-interface-agent-nkanima` | 17 lignes |
| `06-interface-agent-nogee` | 21 lignes |

**La copie avait vingt-six minutes et elle était déjà fausse.**

> *Une livraison par copie se périme en silence.* Ce dépôt porte déjà cette
> note : Rodolf avait testé un correctif sur un binaire copié, et cherché
> pourquoi il ne changeait rien.

Deux domiciles pour un même document finissent **toujours** par diverger, et
personne ne sait lequel fait foi. Les copies ont donc été **retirées**, et il n'y
a plus qu'un seul endroit : à côté de l'application que le document décrit.

📌 C'est la même règle que pour le reste du dépôt : **le fichier a UNE adresse**,
et celle-là dit à quoi il appartient.

---

## Ce que la chaîne `.nkgui` sait faire de ces documents, au 26/09

Mesuré le même jour. Le vocabulaire complet est dans `../Guides/Vocabulaire-nkgui.md`.

| ce que la spécification écrit | état |
|---|---|
| structure, composants, libellés | ✅ **monté** |
| couleur de fond, couleur de texte, rayon | ✅ **peint** |
| un composant nommé, instancié n fois | ✅ **développé avant le montage** |
| une bibliothèque de composants partagée (`include`) | ✅ **résolue** |
| `on = Click` · `on = Changed` · `on = Hover` | ✅ **déclenchés** |
| `on = DoubleClick` `ContextMenu` `Focus` `Blur` `KeyDown` `Wheel` `Submit` `DragStart` `Drop` | 🟡 **reconnus, comptés, sans source** |
| les états (`appearance(Hover)`) | 🟡 **écrits, comptés, non peints** |
| les animations | 🟡 **la section voyage, jamais jouée** |
| `font` `shadow` `stroke` | 🟡 **comptés non peints** |
| rotation, miroir, perspective, opacité, fusion | ❌ **aucun vocabulaire** |

⚠️ **Écrivez quand même ce qui n'est pas tenu.** Ces déclarations voyagent dans
le fichier et se comptent : le jour où elles seront branchées, le document
s'anime **sans être réécrit**. Ce qui n'aurait pas été écrit serait perdu.

---

## La chaîne est fermée depuis le 26/09

```
NKUIDesign  →  .nkgui  →  NKGui / NKEditorKit
```

Mesuré de bout en bout : NKUIDesign écrit, **NkAnimaEditor monte — 40 widgets,
0 rôle inconnu**. L'entrée « `.nkgui` » est dans le dialogue d'export, à côté de
PNG et SVG.

> **Pour les applications qui tournent sur NKGui et NKEditorKit, la source est le
> `.nkgui`, jamais une image.** Une image ne se monte pas : ni rôle, ni
> identifiant, ni comportement.

📌 **Et la cible dépasse le PC** : ces designs visent applications **et jeux**,
sur **PC, web et mobile** — puis HTML/CSS/JavaScript, Java, C#, Qt. Ce sont des
**lecteurs de plus du même document**. C'est ce qui interdit d'écrire un design
en C++ : il ne servirait qu'une seule cible.

### Ce qui bloque encore la fidélité, chiffré

⚠️ **Le placement.** Mesuré par la recette d'écrivain : le document de l'éditeur
pose **42 nœuds sur 42 en ABSOLU**, et **0 en flux**. Le monteur, lui, place au
flux. Un document écrit en flux se monte correctement — la recette le prouve avec
son témoin. **Ce n'est donc pas le monteur : c'est l'éditeur qui ne produit que
de l'absolu.** C'est le manque le plus visible, parce qu'une maquette dense ne
survit pas au flux.
