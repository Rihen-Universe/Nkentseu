# NK3DModeler — spécification d'interface

> Rédigé le **2026-07-31**. Compagnon de `SPECIFICATION.md`, qui définit le
> périmètre ; celui-ci définit **la surface**.
>
> **Trois lecteurs, un seul document** — c'est délibéré :
> - **Rihen**, pour valider ou refuser des choix avant qu'ils ne soient codés ;
> - **l'agent** (moi), comme référence d'implémentation ;
> - **Banani**, pour produire des maquettes — d'où la section 3, écrite en
>   contraintes de disposition plutôt qu'en descriptions littéraires.
>
> Une maquette qui contredirait ce document est une **question à poser**, pas une
> licence artistique : chaque règle ci-dessous répond à un problème constaté.

---

## 1. Principe directeur

> **Rien d'important ne doit exister uniquement dans un journal.**

C'est la règle qui a fait naître ce projet. Aujourd'hui, pour savoir quel
modificateur est actif, quel est le pas d'aimantation, ou combien de sommets sont
sélectionnés, il faut **ouvrir `logs/app.log`**. Toute information qu'un
utilisateur doit connaître pour décider de son prochain geste **est affichée en
permanence** ou accessible en un clic.

Corollaire : une information affichée doit être **vraie en continu**, pas
rafraîchie au coup par coup. Un compteur figé est pire que pas de compteur —
c'est déjà arrivé, un `Draw:0 Tris:0` mort dans le HUD m'a fait conclure à tort à
une scène vide.

---

## 2. Grammaire visuelle — déjà établie, à ne pas réinventer

Ces conventions existent dans le viewport et l'interface doit les **reprendre**,
sinon l'utilisateur apprendrait deux codes pour une seule notion.

| notion | couleur | où c'est déjà appliqué |
|---|---|---|
| axe X / Y / Z | rouge / vert / bleu | gizmos, grille, verrou d'axe |
| **non sélectionné** | noir | sommets, arêtes, faces |
| **sélectionné** | orange `(1, 0,55, 0,05)` | sommets, arêtes, remplissage de face, liseré objet |
| **actif** | blanc | sommet, arête, face active, objet actif |
| lumière | jaune ; teinte claire si active | widgets de lumière |
| erreur / non manifold | à définir — **rien n'existe encore** | — |

**Trois états, pas deux.** La distinction sélectionné / actif est déjà implantée
partout dans le viewport ; les panneaux doivent la respecter : dans une liste, la
ligne active se distingue des lignes seulement sélectionnées.

---

## 3. Disposition

Contraintes de zone, pas d'esthétique. Les proportions sont indicatives ; les
**adjacences** ne le sont pas.

```
┌──────────────────────────────────────────────────────────────────────────┐
│ A  BARRE DE MENUS      Fichier · Édition · Ajouter · Objet · Maillage    │
├──────────────────────────────────────────────────────────────────────────┤
│ B  EN-TÊTE DE VUE                                                        │
│    [Objet ▾] [◆ V E F] │ Orient:[Global ▾] Pivot:[Médian ▾]              │
│    Aimant:[◉ 0,5 grille] │ Affichage:[Solide ▾] │ Matcap:[07 ▾]          │
├───────────────────────────────────────────────┬──────────────────────────┤
│                                               │ D  OUTLINER              │
│                                               │    arborescence de scène │
│                                               ├──────────────────────────┤
│  C  VUE 3D                                    │ E  PROPRIÉTÉS (onglets)  │
│                                               │    ▸ Objet               │
│     ┌──────────────┐                          │    ▸ Modificateurs  ★    │
│     │ F  PANNEAU N │  (repliable, touche N)   │    ▸ Matériau            │
│     │  Transform   │                          │    ▸ Données maillage    │
│     │  Élément     │                          │                          │
│     └──────────────┘                          │                          │
├───────────────────────────────────────────────┴──────────────────────────┤
│ G  BARRE D'ÉTAT   V:8 A:12 F:6 sél.4 │ actif: Cube │ 60 fps │ [message]   │
└──────────────────────────────────────────────────────────────────────────┘
```

### Règles de disposition, et leur raison

1. **B est adjacent à C.** L'en-tête décrit l'état de la vue ; l'éloigner
   obligerait à traverser l'écran des yeux à chaque geste.
2. **Le panneau N est DANS la vue**, superposé, repliable — pas une colonne
   supplémentaire. Il montre l'élément sous la main ; il doit être près de lui.
3. **E est à droite, en onglets**, l'onglet Modificateurs marqué ★ : c'est le
   plus utilisé et le premier livré.
4. **G est sur toute la largeur** et porte les compteurs. Ils sont aujourd'hui
   absents ou faux : c'est un défaut à corriger, pas un détail cosmétique.
5. **Aucune zone flottante par défaut.** Les fenêtres flottantes se perdent
   derrière la vue et rendent l'état invisible — ce qu'on cherche justement à
   éviter.

---

## 4. Le panneau Modificateurs — spécification détaillée

C'est le premier panneau livré, et le plus contraint. Il consomme directement
`NkModifierParams(type, count)`.

```
┌ Modificateurs ───────────────────────────────┐
│ [ + Ajouter un modificateur ▾ ]              │  ← liste des 17 types, groupée
├──────────────────────────────────────────────┤     Générer / Déformer / Normales
│ ▾ ◉ Subdivision de surface        id 1  ⋮    │
│     Niveaux            [ 2 ] ─────────       │  ← généré depuis la table
│     Simple (linéaire)  [ ]                   │
│     [Appliquer] [Dupliquer] [Supprimer]      │
├──────────────────────────────────────────────┤
│ ▾ ◉ Chanfrein                     id 2  ⋮    │
│     Largeur            [ 0,050 ] ────        │
│     Segments           [ 1 ] ─────           │
└──────────────────────────────────────────────┘
```

### Règles dures

- **Aucun widget écrit à la main par type de modificateur.** Les contrôles sont
  déduits de `NkModParam::type` :
  `Bool` → case à cocher · `Int` → champ entier avec incréments ·
  `Float` → curseur borné par `minV`/`maxV` · `Vec3` → trois champs X/Y/Z.
  **Vérification d'acceptation** : ajouter un 18ᵉ modificateur ne doit demander
  **aucune ligne d'interface**. Si ce n'est pas le cas, le panneau est mal écrit.
- **`label` s'affiche, `name` jamais.** `name` est une clé d'animation et de
  fichier ; l'exposer inviterait à la renommer.
- **L'ordre de la pile est l'ordre d'évaluation**, de haut en bas. Le glisser doit
  être possible ; ↑↓ reste obligatoire (accessible, et scriptable).
- **`◉` = activé.** Un modificateur éteint reste visible et grisé — le masquer
  ferait croire qu'il a été supprimé.
- **« Appliquer » est destructif et doit le dire.** Il cuit dans le maillage et
  retire l'entrée. Si le modificateur n'est **pas le premier**, l'interface
  affiche l'avertissement que le moteur remonte déjà (`outWarnNotFirst`) : le
  résultat ne correspondra pas à l'affichage.
- **`id` est visible.** C'est la clé qu'une courbe d'animation utilisera ; la voir
  évite de deviner plus tard.
- **`⋮`** ouvre : marquer un paramètre pour l'animation *(v2)*, copier, épingler.

---

## 5. Table de raccourcis

**Aucun raccourci en dur.** Ils passent par une table `NkShortcutTable`
(NKEditorKit) : identifiant de commande, combinaison, contexte (objet / édition /
global). Trois conséquences, toutes recherchées :
1. l'interface **affiche** le raccourci à côté de chaque commande ;
2. l'utilisateur peut le **changer** ;
3. les conflits sont **détectables** — c'est déjà un problème réel : `Shift+S`
   est pris par « ombrage smooth », ce qui a interdit de l'utiliser pour la pile
   et m'a obligé à des combinaisons moins naturelles.

### Inventaire de l'existant à porter

Relevé sur le code actuel — **50 touches distinctes**, avec modificateurs :

| contexte | raccourcis |
|---|---|
| **global** | `TAB` mode · `Shift+TAB` aimantation · `F` caméra · `Z` affichage · `B` couleur de fond · `N` panneau · `.` pivot · `,` orientation · `Échap` annuler l'opération modale |
| **objet** | `G`/`R`/`S`/`C` gizmo · `Alt+G|R|S` réinitialiser · `A`/`Alt+A` tout/rien · `X`/`Y`/`Z` verrou d'axe · `Ctrl` aimanter · `B` rectangle · `C` cercle · `Ctrl+glisser` lasso · `Shift+clic droit` curseur 3D |
| **édition** | `1`/`2`/`3` sommet/arête/face (`Shift+` combiner) · `E` extruder · `I` insérer · `K` couteau · `W` subdiviser · `M` souder · `X` supprimer · `V` edge split · `J` spin · `Ctrl+R` loop cut · `Ctrl+B` chanfrein arête · `Ctrl+Shift+B` chanfrein sommet · `Ctrl+X` dissoudre · `Shift+Alt+S` to sphere · `Alt+clic` boucle · `Alt+Z` x-ray · `Shift+S`/`Shift+F` smooth/flat |
| **modificateurs** | `F7` ajouter · `F8`/`F9` changer de type · `F10` vider · `[`/`]` régler · `\` modificateur actif · `Shift+\` paramètre suivant · `Shift+↑`/`Shift+↓` déplacer · `Shift+E` activer · `Shift+D` dupliquer · `Shift+Suppr` retirer · `Shift+Entrée` appliquer |
| **vues** | `Numpad 0-9` vues standard · `F1`-`F12` réglages d'ombres et session |

**Écart connu à traiter au portage** : `F7`-`F10` et `[`/`]`/`\` ne sont pas les
raccourcis de Blender — ils datent d'une époque sans panneau. Une fois le panneau
là, la plupart n'ont plus lieu d'être ; les conserver en second rôle est
acceptable, les inventer à nouveau ne l'est pas.

---

## 6. Règles d'interaction

1. **Un clic, un destinataire.** Le clic est arbitré une seule fois : poignée de
   gizmo, puis lumière (distance écran), puis objet/élément. Un clic consommé ne
   redescend jamais. C'est déjà implanté et ce doit rester vrai avec les panneaux
   — un clic dans un panneau ne touche pas la vue.
2. **Rien n'est refusé en silence.** Si une action n'a pas d'effet, on l'exécute
   et on **dit** pourquoi elle ne changera rien. Jurisprudence : déplacer une
   lumière directionnelle est autorisé, et un message explique que l'éclairage ne
   bougera pas.
3. **Toute opération destructive pose un point d'annulation avant.**
4. **Aperçu temps réel des opérations modales** : souris = paramètre continu,
   molette = paramètre entier, clic gauche = confirmer, Échap / clic droit =
   annuler. Rien n'entre dans l'historique tant que ce n'est pas confirmé.
5. **Les panneaux ne dupliquent jamais la logique.** Un bouton émet la **même
   commande** que le raccourci. Sinon l'humain, le rejeu et l'IA divergeront.

---

## 7. Pour Banani — ce qu'on attend d'une maquette

**À produire :** la disposition de la section 3 en trois états — mode objet,
mode édition, panneau modificateurs ouvert avec deux entrées empilées.

**Contraintes à respecter :** les adjacences de la section 3, la grammaire de
couleurs de la section 2, les trois états sélection (noir / orange / blanc), et
le fait que les compteurs de la barre d'état sont **toujours visibles**.

**Libre :** typographie, densité, icônes, rayons, ombres, thème clair/sombre.

**À éviter, parce que ce sont des erreurs déjà payées :** fenêtres flottantes par
défaut ; information disponible seulement au survol ; un panneau modificateurs
dessiné avec des contrôles spécifiques par type — il doit avoir l'air **générique**,
puisqu'il l'est.

---

## 8. Ce que ce document ne fixe pas encore

Assumé, à trancher quand la question se posera vraiment :
- le **thème** (clair / sombre / suivi du système) ;
- le **docking** : NKEditorKit a un défaut connu sur le split droit — à corriger
  avant de promettre des panneaux déplaçables ;
- la **localisation** : l'interface est en français ; l'anglais viendra si le
  besoin apparaît, pas avant ;
- l'**accessibilité** (contraste, taille de police) : à instruire, pas à improviser.
