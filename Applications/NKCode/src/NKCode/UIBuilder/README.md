# UIBuilder — concepteur d'UI par glisser-déposer (WYSIWYG)

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`UIBuilder` permet de **créer des interfaces utilisateur à la souris, sans coder** — façon Qt
Designer, Unreal UMG ou Unity UI Builder. On **glisse des widgets** depuis une palette sur un
**canvas de conception**, on les **dispose, redimensionne et aligne**, on règle leurs **propriétés**
dans un inspecteur, et NKCode produit l'UI (code NKUI généré, ou définition chargée à l'exécution).
C'est la pièce qui rend NKCode utile pour **construire des apps et des jeux**, pas seulement du code.

Cinq éléments :
1. **Palette de widgets** — boutons, panneaux, texte, champs, listes, images, sliders, conteneurs…
2. **Canvas de conception** — poser/déplacer/redimensionner, **guides d'alignement**, magnétisme,
   aperçu fidèle (rendu via NKUI/NKCanvas, donc *ce qu'on voit est ce qu'on aura*).
3. **Arbre de hiérarchie** — la structure des widgets (parent/enfant), réordonnable.
4. **Inspecteur de propriétés** — position, taille, **ancres / layout responsive**, couleurs, texte,
   état… piloté par **NKReflection** : c'est **le** cas d'usage canonique de la réflexion (chaque
   propriété réfléchie devient un champ éditable). → renforce la clé de voûte du projet.
5. **Layout responsive** — ancres, flex/grille, **safe-area**, cohérent avec le toolkit éprouvé
   (`NkoungFrame`-like) : une UI conçue ici s'adapte desktop/mobile/web.

L'UI conçue est **sérialisée** (NKSerialization, un `.nkui`) et **se génère en code NKUI** (ou se
charge en UI retenue). Les **événements** (clic, survol…) se **branchent sur la logique** via
[Blueprint](../Blueprint/README.md)/[Blocks](../Blocks/README.md) (le substrat Graph) ou du code —
on dessine l'écran *et* on câble son comportement, dans le même IDE.

## Responsabilités
- **Palette** + **canvas** de conception (poser/déplacer/redimensionner, guides, magnétisme).
- **Arbre de hiérarchie** des widgets.
- **Inspecteur de propriétés** via **NKReflection** ; édition en direct.
- **Layout responsive** (ancres, flex/grille, safe-area).
- **Persistance** (`.nkui` via NKSerialization) + **codegen** vers NKUI (ou chargement runtime).
- **Liaison événements → logique** (Graph/Blueprint/Blocks ou code).

## Dépend de
**NKUI**/**NKCanvas** (rendu/aperçu), **NKReflection** (inspecteur), **NKSerialization** (sauvegarde),
[Graph](../Graph/README.md)/[Blueprint](../Blueprint/README.md) (liaison événements),
[Codegen](../Codegen/README.md), [Editor](../Editor/README.md).

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
