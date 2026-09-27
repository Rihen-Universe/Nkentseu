# HumanModelingTools — ce qu'on attend de l'atelier de personnages

### Document 4 — Spécification produit (supplément à NKCraft)

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 27/09/2026. Origine : la demande de Rodolf — *« est-ce que dans
> NKCraft on pourrait avoir une section spéciale HumanModelingTools qui permet de
> modéliser des personnages et animaux en partant des parties de mannequin comme
> ceci, où l'utilisateur modifie les paramètres de chaque partie par donnée et
> peut donc aussi sculpter et modeliser chaque partie pour cadrer avec une
> silhouette en particulier, de la tête au pied, même sur le nombre de membres, la
> face, et j'en passe ? »* — accompagnée de vingt-et-une planches de référence.
>
> **C'est un SUPPLÉMENT, pas un remplacement.** Il s'ajoute à `01-produit`,
> `02-interface-humaine` et `03-interface-agent` de NKCraft, qu'il ne contredit
> nulle part. Ordre d'autorité inchangé : `docs/SPECIFICATION.md` et
> `docs/UI_SPEC.md` tranchent le périmètre et les gestes de NKCraft ; **01**
> tranche le produit ; **02** gagne sur **03**. Ce document ne tranche que sur
> HumanModelingTools.
>
> **Compagnons** : **05** (interface pour un humain), **06** (interface pour
> l'agent qui écrit les `.nkgui`).
>
> ⚠️ **EN RÉSERVE.** Décision de Rodolf, 27/09 : *« écris-le comme spécification
> supplémentaire puis on le garde en réserve le temps de terminer avec
> NKUIDesign. »* Aucun code ne s'écrit sur cette base avant que NKUIDesign soit
> fini. Ce document existe pour que le chantier puisse démarrer **sans moi**, et
> sans re-décider ce qui l'est déjà.

---

## 0. En une phrase

**Un personnage ou un animal naît d'un GRAPHE DE PARTIES articulées — chacune
réglable par des mesures nommées et sculptable par-dessus, sans jamais perdre son
réglage.**

---

## 1. Ce que les références montrent, et ce qu'elles imposent

Les vingt-et-une planches fournies ne montrent pas ce qu'on croit au premier
regard. Elles ne montrent **pas un maillage de personnage**. Elles montrent :

1. **Un jeu de volumes séparables.** Crâne, mâchoire, cage thoracique, bassin,
   bras/avant-bras, cuisse/mollet, main, pied — chacun un volume distinct, avec
   un recouvrement et une collerette aux jonctions.
2. **Des articulations NOMMÉES et VISIBLES.** Les bandes rouges ou orange des
   planches *sont* les articulations. Elles ne décorent pas : elles disent où le
   volume se coupe.
3. **Le ventre est une rotule.** La planche *Dummy Pose* de Kouza l'écrit :
   *« Sometimes it's easier to think of the belly as a ball (like other joints)
   to articulate the dummy. »* Le torse est donc **deux** volumes avec une rotule
   entre eux, jamais un bloc.
4. **Le même jeu donne des silhouettes opposées.** Une planche aligne, côte à
   côte, un colosse, une femme élancée, une silhouette fine et un chibi — bâtis
   sur le même vocabulaire de parties. C'est la preuve par l'image que la
   silhouette est un jeu de **paramètres**, pas un maillage différent.

📌 **Ce que ça impose** : la donnée première n'est pas un maillage, c'est un
**graphe**. Le maillage en est une conséquence.

---

## 2. Les quatre décisions, et pourquoi chacune coûte cher si on la rate

### D1 — Un GRAPHE DE PARTIES, jamais un squelette humanoïde en dur

Chaque partie porte un volume et des **sockets** : des points d'attache nommés,
avec une orientation. Un socket accepte une autre partie.

Un quadrupède, une araignée, un personnage à quatre bras, une queue, une paire
d'ailes : **même donnée, graphe différent**. Rien à ajouter au code.

> 🔴 **LE PIÈGE, ET IL EST IRRÉVERSIBLE.** Écrire `BrasGauche`, `BrasDroit`,
> `JambeGauche`… en énumération. Ça marche pour un humain, ça ferme la porte au
> premier centaure — et rouvrir cette porte veut dire réécrire tout ce qui s'est
> appuyé dessus entre-temps. NKCraft connaît déjà cette leçon sous une autre
> forme : *« la clé est un NOM, jamais une position »*.
>
> Le mot de Rodolf, dans sa demande, est explicite : *« même sur le nombre de
> membres »*. C'est une exigence, pas un souhait.

### D2 — 🔴 LE SCULPT EST UNE COUCHE, PAS UNE CUISSON

C'est **la** décision. Tout le reste en dépend.

L'utilisateur règle la longueur d'une cuisse, puis la sculpte pour lui donner du
galbe. Puis il change la longueur. **Le sculpt doit suivre.**

Donc le sculpt se range comme un **déplacement relatif à la base paramétrique**,
exprimé dans l'espace propre de la partie — jamais fondu dans les positions des
sommets.

> **Cuit, chaque paramètre devient à sens unique** : on façonne une fois, puis on
> est enfermé. C'est la différence exacte entre un jouet et un outil. Et le
> défaut ne se voit pas le jour où on l'écrit : il se voit le jour où quelqu'un
> veut retoucher une proportion, trois semaines plus tard, sur un personnage
> qu'il a déjà sculpté.

**Corollaire non négociable** : un déplacement relatif exige une **topologie
stable** sous lui. La retopologie passe donc **avant** la sculpture, comme elle
passe déjà avant les UV dans ce dépôt.

### D3 — Deux régimes de jonction, et commencer par le premier n'est pas rogner

| régime | ce qu'il demande | état |
|---|---|---|
| **stylisé / mécanique** — *toutes les références fournies* | les parties **restent séparées** : recouvrement, collerette, et une rotule visible | faisable sur l'existant |
| organique continu | coudre les parties puis **retopologiser** la jonction | un chantier à part entière |

Les vingt-et-une planches sont **toutes** du premier type. Commencer par lui,
c'est viser la cible choisie — pas accepter un compromis.

### D4 — Une mesure porte son UNITÉ et sa RÉFÉRENCE

`longueur = 42` ne veut rien dire. Quarante-deux quoi, et par rapport à quoi ?

Chaque mesure se déclare soit en **absolu**, soit en **fraction d'une mesure de
référence** (le plus souvent la hauteur totale, ou la hauteur de tête — l'unité
que les planches de proportions emploient elles-mêmes).

> Un chiffre qui voyage sans sa condition est un chiffre faux dès qu'il change de
> personnage. Un `42` juste sur une brute est absurde sur un chibi, et rien ne le
> signalera.

---

## 3. Ce qu'on fait dans HumanModelingTools

1. **Composer** — poser des parties, les attacher par leurs sockets, en retirer,
   en dupliquer en miroir.
2. **Mesurer** — régler chaque partie par des valeurs nommées : longueur,
   circonférence en trois points (haut, milieu, bas), galbe, torsion, rotation du
   socket.
3. **Sculpter** — par-dessus, sans perdre le réglage (D2).
4. **Cadrer sur une silhouette** — poser une image de référence et ajuster les
   parties jusqu'à ce que la silhouette colle. De la tête aux pieds.
5. **Poser** — articuler le mannequin pour vérifier que les volumes ne se
   traversent pas dans les poses extrêmes. **Poser n'est pas animer** : c'est un
   contrôle de conception, l'animation appartient à NkAnima.
6. **Faire naître le visage** — la face est un sous-graphe de parties (crâne,
   mâchoire, nez, orbites, oreilles), réglé et sculpté comme le reste.

---

## 4. Frontières — ce que HumanModelingTools NE fait PAS

| hors périmètre | où ça vit |
|---|---|
| l'animation, le squelette d'animation, le facial animé | **NkAnima** |
| la mise en scène, l'éclairage, le rendu final | **NKScena** |
| la texture et les matériaux | NKCraft, espaces existants |
| la génération par IA d'un personnage entier | NKCraft, chemin IA existant |

⚠️ **Mais le graphe de parties EST un rig.** Il fournit à NkAnima un squelette
déjà nommé et hiérarchisé, gratuitement. C'est un cadeau à ne pas gâcher : les
noms de sockets doivent être choisis en pensant à celui qui animera.

---

## 5. Le témoin — ce qui décide que le chantier tient

> 🔴 **TROIS PARTIES, UN PARAMÈTRE, UN SCULPT.**
>
> Je pose une cuisse, un mollet et un pied. Je sculpte du galbe sur la cuisse.
> **Puis je change sa longueur.**
>
> **Le sculpt suit, et la jonction avec le mollet reste propre.**

Il est petit exprès. Vert, tout le reste est du volume de travail. Rouge, rien de
ce qu'on empilera dessus ne tiendra — et on l'aura su au premier jour plutôt
qu'au troisième mois.

**Deux témoins de second rang**, à tenir dès que le premier est vert :

- **T2** — le même graphe, deux jeux de mesures, deux silhouettes
  reconnaissables (une brute, un chibi) **sans toucher à un seul sommet**.
- **T3** — un graphe à **cinq membres** se compose, se pose et se rend, sans
  qu'aucune ligne de code n'ait été ajoutée pour lui.

T3 est le témoin de D1 : c'est lui qui prouve que le graphe est bien une donnée.

---

## 6. Ce qui existe déjà et sur quoi ça s'appuie

Relevé du 27/09, dans `NKCraft` (ex-NK3DModeler) :

- l'édition de maillage : biseau, extrusion en creux, coupe d'anneau, avec leurs
  lois déjà mesurées ;
- la **sculpture**, qui agit à la souris ;
- les primitives et leur catalogue par nom ;
- la **retopologie** en quadrangles, et la règle « retopologier d'abord » ;
- le viewport 3D en rendu hors-écran ;
- le panneau IA, branché.

Ce qui manque et qui est propre à ce chantier : le **graphe de parties**, les
**sockets**, la **couche de déplacement** et les **mesures nommées**.

---

## 7. Phasage proposé

| phase | contenu | témoin |
|---|---|---|
| **P1** | le graphe, les sockets, trois parties, les mesures | **le témoin §5** |
| **P2** | la couche de sculpt et sa survie aux paramètres | T2 |
| **P3** | le miroir, la duplication, les graphes non humanoïdes | T3 |
| **P4** | l'image de référence et le cadrage de silhouette | une silhouette cadrée à l'œil |
| **P5** | le sous-graphe du visage | un visage réglé, puis sculpté, puis re-réglé |
| **P6** | la pose de contrôle et la détection d'interpénétration | une pose extrême sans volume traversé |

⚠️ **P2 ne se saute pas.** Livrer P1 et P3 sans P2 donnerait un outil qui
impressionne en démonstration et que personne ne peut employer deux fois sur le
même personnage.

---

## 8. Décisions prises

| # | décision | par | date |
|---|---|---|---|
| D1 | graphe de parties, jamais d'énumération de membres | Rodolf (*« même sur le nombre de membres »*) | 27/09 |
| D2 | le sculpt est une couche relative, jamais cuit | proposée et argumentée, **à confirmer** | 27/09 |
| D3 | commencer par le régime stylisé (parties séparées) | proposée, **à confirmer** | 27/09 |
| D4 | toute mesure porte unité et référence | proposée, **à confirmer** | 27/09 |
| D5 | le chantier reste **en réserve** jusqu'à la fin de NKUIDesign | Rodolf | 27/09 |

📌 **D2, D3 et D4 attendent le mot de Rodolf.** Elles sont écrites pour qu'il
puisse trancher sur pièce, pas pour être tenues pour acquises.

---

*Documents compagnons : `05-interface-humaine-humanmodelingtools.md`,
`06-interface-agent-humanmodelingtools.md`.*
