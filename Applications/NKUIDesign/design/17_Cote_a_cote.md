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

### E2 ✅ Le badge de composant se coupait en plein mot — CORRIGÉ le 2026-09-02

Hiérarchie, rangée `Bouton_Connexion` : notre badge affichait « Butto » tronqué
par le bord du panneau. Dans la planche, c'est **l'inverse : le NOM cède
(« Bouton_Conn… »), le badge ne se coupe jamais** — ancré au bord droit, entier.

**Corrigé au bon étage** : le libellé est dessiné par le composant du kit,
l'overlay du badge passe après — aucun des deux ne pouvait céder proprement
seul. Nouveau point de greffe `rowRightReserve` (NkTreeView) : la ligne réserve
la largeur du badge AVANT de poser le libellé, qui s'ellipse contre elle. Au
passage, l'ancienne position du badge (indentation + icônes + largeur du texte,
recalculées à la main) disparaît — elle était fragile ET fausse.

Preuves : capture `captures/2026-09-02_hierarchie_e2_corrige.png`
(« Bouton_Conn… (Button) », badge entier) ; recette `--recette-edition` site 4 —
le rectangle du libellé perd exactement les 40 px demandés, et la mutation
« le dessin ignore la réserve » fait échouer le cas (vérifié, 15/16 sous
mutation). Le témoin de rendu ne couvre pas la Hiérarchie : ici, la preuve
visuelle est la capture, pas lui.

### E3 ✅ CORRIGÉ (02/09) — l'ouverture cadre la première page dans le viewport UTILE

Notre capture : le rail flottant recouvre le bord gauche de la planche
Connexion. Dans 22.0, la première planche démarre avec une gouttière franche à
droite du rail. Le rail est à sa place (bord gauche de la toile) — c'est le
**cadrage d'ouverture** qui ne lui réserve pas sa gouttière.
→ **LIVRÉ** : `AjusterSur` porte des bandes réservées (`reserveGauche` =
rail, `reserveBas` = sélecteur de zoom) qui valent pour TOUS les ajustements
(Ctrl+0..3 compris — ajuster la sélection sous le rail eût été le même
défaut) ; l'ouverture cadre **la première page, plafonnée à 100 %** — pas
l'englobant du document, qui donnait 5 % et trois timbres-poste (mesuré).
Deux leçons payées en chemin : le premier essai courait sur un **viewport
dégénéré** d'avant l'installation du dock (la trace de cadrage, ajoutée pour
départager, a montré « page 240×520 → zoom 0.05 » : c'est le viewport qui
mentait, pas la page) ; le cadrage attend désormais un viewport ≥ 300×300 et
**journalise ce qu'il a fait**. Preuve : `captures/2026-09-02_ouverture_cadree.png`
— planche entière, 100 %, hors du rail et du sélecteur.*

### E4 ✅ CONFORME PAR DÉCISION (Rodolf, 02/09) — nos deux rangées gagnent

Planche : une seule rangée de six icônes. Chez nous : deux rangées étiquetées
« H » / « V », quatre icônes chacune. **Rodolf a tranché : les deux rangées
restent.** Ce n'est pas un écart à corriger, c'est un choix assumé contre la
planche — et il est écrit ICI, là où l'écart était nommé, précisément pour
que le prochain lecteur ne « corrige » pas vers la planche en croyant bien
faire. *Une décision qui ne vit pas à l'endroit où la question se repose est
une décision qui se re-perd.*

### E5 ✅ CORRIGÉ (02/09) — l'icône du segment actif était accent sur accent

Planche : chaque segment du sélecteur de mode porte icône **et** libellé, actif
compris (« A Design »). Chez nous : « Design » actif est texte seul, les trois
inactifs ont leur icône. Petit, mais c'est le genre d'asymétrie qui « fait
bricolé » sans qu'on sache dire pourquoi.
→ **LIVRÉ (02/09)** : la cause n'était pas une icône absente mais une icône
**accent sur accent** — dessinée, invisible. Elle suit désormais l'encre de
son libellé (`onAccent` sur le segment actif). Visible sur
`captures/2026-09-02_ouverture_cadree.png` (« A Design »).*

### E6 ✅ La capture avec sélection EXISTE (`--selectionner=`, 02/09) — et sa nécessité a été payée le jour même

La planche 091913 photographie un **bouton sélectionné** ; notre première
capture photographiait la **Toile** (états vides). Comparer l'état vide de
l'un à l'état plein de l'autre serait le défaut « données dégénérées » en
image — et il a été **payé en vrai avant d'être corrigé** : le passage du
retrait des champs de 6 à 8 px a traversé **deux témoins verts** (le flux, qui
n'a pas de glyphes en headless ; la capture, dont l'inspecteur était vide).

`--capture=<f.png> --selectionner=Bouton_Connexion` met l'inspecteur dans
l'état PLEIN : `captures/2026-09-02_inspecteur_selection.png` (CIBLE,
DISPOSITION avec valeurs, REMPLISSAGES avec pastille + hex + % + poubelle +
œil). La preuve A/B du retrait 6→8 : diff pixel par pixel **non vide et
confiné à la colonne inspecteur** (zéro pixel ailleurs). Les angles morts du
témoin de flux sont désormais écrits dans son en-tête (`TemoinRendu.h`) :
pour un changement de panneau, la preuve est la **paire de captures**, jamais
« témoin identique » cité seul.

---

## Ce qui est CONFORME et qu'on ne retouche pas

Structure générale ✓ (en-tête logo/menus/titre centré/contrôles, onglets avec
icône + ×, hiérarchie PAGES/COMPOSANTS, rail flottant arrondi avec variantes,
sélecteur de mode centré, inspecteur à onglets Design/Widget/Behavior avec rail
d'icônes à droite, pied Console/Aperçu + « Prêt », étiquettes de planche
grises, sélecteur de zoom en pastille). Le squelette de la planche est là ;
ce qui reste est du réglage, pas de la structure.
