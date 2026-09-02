# 17 — Côte à côte : l'application contre les planches

> **But** : mettre la vraie fenêtre (capture `--capture=`) en face des planches
> Banani, et **nommer chaque écart** — pour que Rodolf pointe ce qui le gêne au
> lieu de le deviner. Lecture : cinq minutes.
>
> **Régénérer la capture** : `NKUIDesign.exe --capture=<fichier.png>` — ouvre sa
> propre fenêtre, photographie **son** backbuffer à la frame 8 (readback du
> noyau, pas une capture d'écran de l'OS), ferme. Une instance déjà ouverte
> n'est ni vue, ni touchée.

| Pièce | Fichier |
|---|---|
| Capture du 2026-09-02 (fenêtre par défaut, document du disque) | `captures/2026-09-02_fenetre_defaut.png` |
| Planche fenêtre complète | `planches/091913_inspecteur_v1.png` |
| Planche toile en mode Design | `planches/22.0_canvas_design.png` |

---

## D'abord, le NON-écart : les couleurs

Les planches sont **violettes** (accent indigo, toile claire dans 22.0) ; notre
fenêtre est **GitHub Dark Pro** (accent `#1F6FEB`, toile sombre). **Ce n'est pas
un écart** : la règle de la maison dit *« les maquettes donnent la géométrie,
pas les couleurs »* — le thème GitHub est décidé pour toutes les applications.
Toute ligne ci-dessous parle donc de **géométrie, proportions, comportement**.

---

## Les écarts, nommés — du plus gênant au plus petit

### E1 🔴 La respiration de l'inspecteur — c'est le chantier des 225 sites, vu à l'œil

Planche : rangées ~30 px, gouttières régulières, une section = un bloc qui
respire. Chez nous : rangées 26 px, gouttières de 3 à 8 px selon l'endroit, et
les textes d'aide (« calculée — jamais écrite dans le document », « Le parent
n'est pas en anchor… ») s'étalent sur deux lignes pleines qui collent aux
champs. **C'est exactement ce que la mesure du document 16 chiffre** : 34
valeurs d'espacement en mise en page. L'œil de la planche trouve un rythme ;
le nôtre n'en trouve pas.
→ *Traitement : la migration site par site déjà décidée (doc 16), AVEC capture
avant/après par tranche.*

### E2 🔴 Le badge de composant se coupe en plein mot

Hiérarchie, rangée `Bouton_Connexion` : notre badge affiche « Butto » tronqué
par le bord du panneau. Dans la planche, c'est **l'inverse : le NOM cède
(« Bouton_Conn… »), le badge ne se coupe jamais** — il est ancré au bord droit,
entier, et l'ellipse mange le nom. Une étiquette coupée en plein mot se lit
comme un bug, pas comme une étiquette.
→ *Traitement : réserver la largeur du badge d'abord, tronquer le nom ensuite.*

### E3 🟠 La vue par défaut ouvre le document SOUS le rail d'outils

Notre capture : le rail flottant recouvre le bord gauche de la planche
Connexion. Dans 22.0, la première planche démarre avec une gouttière franche à
droite du rail. Le rail est à sa place (bord gauche de la toile) — c'est le
**cadrage d'ouverture** qui ne lui réserve pas sa gouttière.
→ *Traitement : `AjusterSur` à l'ouverture doit exclure la bande du rail (et
celle du sélecteur de zoom en bas à droite).*

### E4 🟠 L'alignement : une rangée dans la planche, deux chez nous

Planche : ALIGNEMENT = **une seule rangée de six icônes** (3 H puis 3 V),
compacte. Chez nous : deux rangées étiquetées « H » / « V » avec quatre icônes
chacune. Nos icônes en plus (répartir) ne sont pas le problème — la question
est la hauteur dépensée : deux rangées + étiquettes là où la planche tient en
une.
→ *À trancher par Rodolf : garder nos deux rangées (plus lisibles ?) ou serrer
comme la planche.*

### E5 🟡 Le segment actif a perdu son icône

Planche : chaque segment du sélecteur de mode porte icône **et** libellé, actif
compris (« A Design »). Chez nous : « Design » actif est texte seul, les trois
inactifs ont leur icône. Petit, mais c'est le genre d'asymétrie qui « fait
bricolé » sans qu'on sache dire pourquoi.

### E6 🟡 Ce que cette capture NE PEUT PAS juger — et la capture qui manque

La planche 091913 photographie un **bouton sélectionné** : ancrage en widget
9 points, APPARENCE avec pastilles de couleur + hex, BORDS R/Brd, TYPOGRAPHIE
remplie, poignées + badge « Bouton » sur la toile. Notre capture photographie
la **Toile** (rien de sélectionné) : ces sections montrent leurs états vides —
qui sont peut-être justes, peut-être pas. **Comparer l'état vide de l'un à
l'état plein de l'autre serait le défaut « données dégénérées » en image.**
→ *Prochaine capture : le même document avec `Bouton_Connexion` sélectionné
(il faudra un drapeau `--selectionner=<nom>` ou un geste enregistré).*

---

## Ce qui est CONFORME et qu'on ne retouche pas

Structure générale ✓ (en-tête logo/menus/titre centré/contrôles, onglets avec
icône + ×, hiérarchie PAGES/COMPOSANTS, rail flottant arrondi avec variantes,
sélecteur de mode centré, inspecteur à onglets Design/Widget/Behavior avec rail
d'icônes à droite, pied Console/Aperçu + « Prêt », étiquettes de planche
grises, sélecteur de zoom en pastille). Le squelette de la planche est là ;
ce qui reste est du réglage, pas de la structure.
