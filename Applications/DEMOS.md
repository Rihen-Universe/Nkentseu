# Les démos lançables

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**Créé le 25/09/2026, à la demande de Rodolf : « en branchant, si tu peux, crée
des démos que je peux tester, après je serais content. »**

Une démo est un **exécutable qui s'ouvre et montre**, sans variable
d'environnement, sans argument obligatoire, sans fichier à préparer. Elle est
nommée par **ce qu'elle montre**, jamais par un numéro — `--demo=2` s'est
décalé à chaque fusion et deux bancs ont fini sur le même indice le même jour.

⚠️ **La troisième colonne est la plus utile.** Elle dit quoi **chercher**, pas
quoi admirer. Un banc vert au-dessus d'une chaîne cassée, ce dépôt en a déjà
collectionné : 138 verts contre trois infidélités que personne ne voyait.

⚠️ **Jamais poussées sur GitHub** (règle permanente sur les exécutables).

---

| démo | ce qu'on doit voir | ce qui prouverait que c'est cassé |
|---|---|---|
| **`nkdemodepliageuv`**<br>`Build/Bin/Release-Windows/nkdemodepliageuv/`<br>*lancer depuis la racine de l'arbre* | Un objet **généré** qui tourne lentement, couvert d'un **damier** (gris clair / gris foncé, liseré orange tous les 4 carreaux). Un bandeau pétrole en haut donne l'état : nombre d'îlots, triangles par îlot, distorsion d'aire et d'angle, recouvrement, temps. **ESPACE** bascule `[BRUT]` ↔ `[DEPLIE]`. Le nom du fichier montré est affiché à droite. | • `[BRUT]` et `[DEPLIE]` donnent **la même image** → les UV calculées n'arrivent pas au GPU : le câblage est coupé entre le solveur et le rendu.<br>• Le damier est un **confetti** de carreaux minuscules sans continuité → le dépliage a éclaté le maillage en un îlot par triangle. **C'est l'état mesuré aujourd'hui** (2 115 îlots pour 2 156 triangles) : c'est attendu, et c'est le défaut à corriger.<br>• Un **REFUS** s'affiche avec son nom et son chiffre d'Euler → le maillage n'est pas dépliable tel quel. Ce n'est pas une panne de la démo.<br>• La fenêtre **fige plusieurs dizaines de secondes** à l'ouverture → elle est tombée sur le maillage TripoSR (42 s mesurées), pas sur celui de famille (267 ms). |

## État honnête au 25/09/2026

Le dépliage **est branché** — pour la première fois il est appelable depuis
autre chose qu'un banc — et il **ne passe pas** les critères :

| maillage | temps | îlots | recouvrement | verdict |
|---|---|---|---|---|
| famille (`000.obj`, 2 156 tri) | 267 ms | 2 115 (**1,0 tri/îlot**) | 0 % | **ROUGE** — confetti |
| TripoSR (`table.glb`, 60 478 tri) | 42 336 ms | 1 | **151 %** | **ROUGE** — atlas replié sur lui-même |

Le solveur n'est pas en cause : sa distorsion d'angle vaut 7,04° en moyenne sur
le maillage TripoSR, dans les clous. **Ce qui manque est une stratégie de
coupe** — l'arbre couvrant du dual donne une découpe *valide* (un disque) et
*mauvaise*. C'est le prochain lot, et c'est exactement ce que `SmartUnwrap`
devait faire dans le `NkUVEditor` qui n'a jamais eu de corps.

## Démos à venir (ce lot)

| démo | ce qu'on doit voir |
|---|---|
| **l'éditeur UV** | on ouvre, on déplace un îlot, la texture suit sur l'objet. Pas encore écrit : les opérations doivent d'abord vivre dans `NKRenderer/Mesh/` sur `NkEditMesh`, et le panneau dans `NKEditorKit` pour que NK3DModeler, NkAnimaEditor et NKScena l'hébergent à l'identique. |
