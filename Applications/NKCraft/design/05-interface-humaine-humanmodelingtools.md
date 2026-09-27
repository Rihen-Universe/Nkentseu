# HumanModelingTools — l'interface, pour un humain

### Document 5 — Spécification d'interface côté humain (supplément à NKCraft)

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 27/09/2026.
> Compagnon de **04** (produit) et de **06** (agent).
>
> **Autorité** : `docs/UI_SPEC.md` gagne sur toute question de disposition, de
> palette et de gestes de NKCraft ; **02** gagne sur ce document pour tout ce qui
> est commun à l'application ; ce document ne tranche que l'**espace de travail
> HumanModelingTools**. Et **si 05 et 06 se contredisent, 05 gagne.**
>
> ⚠️ **EN RÉSERVE** jusqu'à la fin de NKUIDesign (04 §8, D5).

---

## 1. Ce que c'est, dans l'application

**Un espace de travail de plus**, au même rang que Modélisation, Sculpture, UV,
Textures — pas une fenêtre séparée, pas un mode caché.

> ⚠️ **PAS UNE FENÊTRE À PART, ET C'EST UNE DÉCISION.** Un atelier de personnages
> dans sa propre fenêtre devient un logiciel dans le logiciel : il refait son
> viewport, sa navigation, ses raccourcis, sa sélection — quatre choses que
> NKCraft fait déjà, et qui divergeraient. L'espace de travail réutilise le
> viewport, les gestes Blender et la palette d'UI_SPEC **sans rien redéfinir**.

---

## 2. La disposition

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Fichier  Édition  ...  [Modélisation│Sculpture│UV│Textures│PERSONNAGE]    │
├────────────────┬──────────────────────────────────────┬──────────────────┤
│ PARTIES        │                                      │ MESURES          │
│ ▾ racine       │                                      │ ── Cuisse G ──   │
│   ▾ bassin     │          le viewport de NKCraft      │ longueur   42,0  │
│     ▾ cuisse G │          (inchangé : mêmes gestes,   │ haut       18,0  │
│       ▾ mollet │           même navigation)           │ milieu     15,5  │
│         └ pied │                                      │ bas        11,0  │
│     ▸ cuisse D │                                      │ galbe       0,30 │
│   ▾ thorax     │                                      │ torsion     0,0  │
│     ▸ bras G   │                                      │ ── socket ──     │
│     ▸ bras D   │                                      │ rotation  (x,y,z)│
│   ▸ crâne      │                                      │                  │
├────────────────┤                                      │ ── couche ──     │
│ BIBLIOTHÈQUE   │                                      │ sculpt   ● actif │
│ [crâne][mâch.] │                                      │ [réinitialiser]  │
│ [thorax][bass.]│                                      │                  │
│ [bras][avant-] ├──────────────────────────────────────┤                  │
│ [cuisse][moll.]│ [Composer│Mesurer│Sculpter│Cadrer│Poser]                │
│ [main][pied]   │                                      │                  │
└────────────────┴──────────────────────────────────────┴──────────────────┘
```

| zone | contenu | pourquoi |
|---|---|---|
| **Parties** (gauche haut) | l'arbre du graphe : chaque nœud une partie, chaque enfant attaché à un socket nommé | c'est **la** donnée (04 §2, D1) ; elle doit se voir |
| **Bibliothèque** (gauche bas) | les parties disponibles, par famille | on compose en glissant |
| **Mesures** (droite) | les valeurs nommées de la partie sélectionnée, **avec leur unité** | 04 §2, D4 |
| **Modes** (bas) | Composer · Mesurer · Sculpter · Cadrer · Poser | cinq verbes, cinq gestes |
| viewport | celui de NKCraft, intact | §1 |

---

## 3. Les cinq modes

### 3.1 Composer

Glisser une partie de la bibliothèque **sur un socket libre** l'attache. Les
sockets libres s'affichent comme de petites rotules ; celui qui accepte la partie
tirée **s'allume**.

- `Suppr` retire la partie **et sa descendance**, après confirmation qui **nomme
  ce qui part** (« retirer `cuisse G` et ses 2 descendants ? »).
- `Ctrl+M` duplique en miroir sur le socket symétrique.

> ⚠️ **LE MIROIR EST UN LIEN, PAS UNE COPIE — jusqu'à ce qu'on le casse.** Régler
> la cuisse gauche règle la droite, tant que le lien tient. Une icône de chaîne
> le montre et le rompt. Sans le lien, on règle tout deux fois et les deux
> divergent d'un dixième sans que personne ne le voie ; sans la rupture, on ne
> peut pas faire un personnage asymétrique — et les deux sont des besoins réels.

### 3.2 Mesurer

Le panneau de droite. Chaque champ porte **son unité**, affichée, jamais
sous-entendue (04 §2, D4) : `42,0 cm` ou `0,180 H` (fraction de la hauteur
totale). Une bascule en tête du panneau change l'affichage sans changer la donnée.

- tirer sur un champ le fait varier en continu, le modèle suit **pendant** le
  geste ;
- une mesure liée par le miroir porte la chaîne.

> ⚠️ **AFFICHER L'UNITÉ N'EST PAS DU CONFORT.** Un panneau qui montre `42,0` tout
> court oblige chaque utilisateur à se souvenir du régime en cours. Il s'en
> souviendra — jusqu'au jour où il ouvrira le fichier d'un autre.

### 3.3 Sculpter

Les outils de sculpture de NKCraft, **inchangés**, appliqués à la partie
sélectionnée.

Ce qui change : le panneau de droite montre une section **Couche**, avec l'état
du déplacement et un bouton `Réinitialiser` qui rend la partie à sa forme
paramétrique pure.

> 🔴 **CE BOUTON EST LA PREUVE VISIBLE DE D2.** S'il ne peut pas exister — c'est
> qu'on a cuit le sculpt, et que le chantier est déjà perdu. Son absence dans une
> maquette doit alerter autant qu'un plantage.

### 3.4 Cadrer

Poser une image de référence (face, profil, trois quarts) sur un plan, et régler
les parties jusqu'à ce que la silhouette colle.

- l'image se place, s'échelonne et s'opacifie ;
- un mode **silhouette** peint le modèle en aplat noir, pour comparer des contours
  et non des détails.

> ⚠️ **L'APLAT EST L'OUTIL, PAS UN EFFET.** Comparer un modèle éclairé à un dessin
> au trait, c'est comparer deux choses différentes : l'œil suit les ombres et rate
> le contour. C'est justement le contour qu'on cadre.

### 3.5 Poser

Articuler pour **vérifier**, pas pour animer (04 §4).

- tirer une rotule fait tourner la partie et sa descendance ;
- un contrôle d'**interpénétration** marque en rouge les volumes qui se
  traversent ;
- `Repos` rend la pose neutre. La pose **n'est pas enregistrée dans le
  personnage** : c'est un contrôle, et le laisser croire ferait exporter un
  personnage figé dans une pose de test.

---

## 4. Les couleurs d'information

Celles de la famille, sans en ajouter :

| ce qu'on montre | jeton |
|---|---|
| socket libre, prêt à recevoir | `@accent` |
| socket occupé | `@info.neutre` |
| mesure liée par le miroir | `@info.lien` |
| partie sculptée (couche non vide) | `@info.modifie` |
| volumes qui se traversent (mode Poser) | `@info.alerte` |

> ⚠️ **AUCUNE COULEUR EN DUR.** Contrôle §25 du doc 3 de la famille : les
> documents ne portent que des jetons.

---

## 5. Ce qui manque encore, et qu'il faudra trancher

1. **Le nom français de l'espace.** Le menu de la famille est en français
   (`Modélisation`, `Sculpture`) ; `HumanModelingTools` détonnerait. Proposition :
   **Personnage**. Le nom de code du module reste `HumanModelingTools`.
2. **Les animaux et le mot « Human ».** Rodolf demande *« personnages et
   animaux »* : le nom du module dit « Human » et le périmètre dit le contraire.
   À trancher — le nom, ou le périmètre.
3. **Le nombre de mesures par partie.** Trois circonférences suffisent-elles, ou
   faut-il un profil ? Trois est le pari, et il se mesurera à l'usage.
4. **L'ordre des modes.** Composer avant Mesurer paraît naturel ; à vérifier sur
   quelqu'un qui n'a pas écrit la spécification.

---

*Documents compagnons : `04-produit-humanmodelingtools.md`,
`06-interface-agent-humanmodelingtools.md`.*
