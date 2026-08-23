# Divergences mesurées entre les documents et les décisions prises

> Produit le 2026-08-23 sur `Nkentseu-merge`, branche `main`, commit de base
> `9b31cc8f`. **Ce fichier ne corrige rien** : il liste, avec fichier et ligne,
> ce qui est affirmé et quelle décision c'est contredit.
>
> Méthode : `git grep` sur les **747 markdown suivis par git** (pas de balayage
> du disque : les fichiers non suivis, notamment `Build/_jenga-embed/`, sont
> hors périmètre). Chaque ligne retenue a été **relue dans son contexte** avant
> d'être classée ; les homonymes ont été écartés à la main (exemple réel :
> `NKPlatform/Readme.md:867` contient `0xDDCCBBAA`, que `grep DCC` remonte).

---

## Décision 1 — trois applications distinctes communiquant par fichiers

**Énoncé.** NK3DModeler (modélisation, sculpture), NkAnimaEditor (animation,
VFX), NKScena (mise en scène). Elles communiquent **par fichiers**.

### 1.1 Le document qui dit l'inverse, contredit par son propre voisin

| fichier | ligne | ce qui est affirmé |
|---|---|---|
| `Applications/NK3DModeler/ROADMAP.md` | 8 | « L'application est **un seul DCC** (pas de séparation modeleur / animation). » |
| `Applications/NK3DModeler/docs/SPECIFICATION.md` | 94 | tableau « Hors produit (v1) » : « Animation, rigging \| module dédié \| **NkAnima** » |

Les deux fichiers sont dans le même dossier. Le ROADMAP (3 481 lignes, modifié
le **2026-08-23**) porte la phrase fausse en **ligne 8**, c'est-à-dire dans le
premier écran que lit un agent qui reprend. La SPECIFICATION (458 lignes,
2026-08-08) dit la bonne chose et **explique pourquoi**. Le document faux est le
plus lu et le plus récent : c'est le pire des deux cas.

### 1.2 Le trio nommé, mais avec les mauvais membres

| fichier | ligne | ce qui est affirmé |
|---|---|---|
| `Applications/NK3DModeler/docs/UI_SPEC.md` | 625-631 | « ## 11. Socle partagé — **NK3DModeler, Nogee, NkAnima** » puis « **le même principe pour les trois applications** », tableau à trois entrées : NK3DModeler / **Nogee** / **NkAnima** |

C'est le **seul** document du dépôt qui pose explicitement un trio. Il se trompe
sur deux des trois membres : il met **Nogee** (l'éditeur du moteur de jeu) à la
place de NKScena, et il nomme **NkAnima** — la bibliothèque — là où l'application
est **NkAnimaEditor**. Contredit la décision 1 *et* la décision 2.

### 1.3 NKScena n'existe nulle part

**Mesure.** `NKScena` apparaît dans **2 fichiers sur 747**, et dans les deux cas
en passant, au fil d'une phrase sur un autre sujet :

| fichier | ligne | contexte |
|---|---|---|
| `Kernel/Runtime/NKAnimation/ROADMAP.md` | 26 | « permet à NkAnima, PV3DE, Noge et NKScena d'animer sans… » |
| `Kernel/Runtime/NKRenderer/ROADMAP.md` | 920 | « NkAnima, PV3DE et NKScena devaient tirer **tout** le… » |

Il n'y a **aucun** `Applications/NKScena/`, aucun README, aucune spécification,
aucune roadmap. La troisième application de la décision 1 n'a pas une seule ligne
de documentation propre. Ce n'est pas une contradiction : c'est un **trou**, et
c'est probablement plus grave, parce qu'un trou ne se détecte pas à la lecture.

### 1.4 Les documents de présentation ignorent les trois applications

**Mesure** (comptage d'occurrences fichier par fichier, vérifié) :

| fichier | dernière modif | NK3DModeler | NkAnimaEditor | NKScena | Noge |
|---|---|---|---|---|---|
| `README.md` | 2026-08-12 | 0 | 0 | 0 | 7 |
| `PRESENTATION_TECHNIQUE.md` | 2026-06-14 | 0 | 0 | 0 | 13 |
| `ECOSYSTEM.md` | 2026-06-22 | 0 | 0 | 0 | 15 |
| `ECOSYSTEM_OVERVIEW.md` | 2026-06-29 | 0 | 0 | 0 | 6 |
| `EXPLICATION_SIMPLE.md` | 2026-06-14 | 0 | 0 | 0 | 14 |
| `ARCHITECTURE.md` | 2026-06-14 | 0 | 0 | 0 | 9 |
| `docs/ARCHITECTURE.md` | 2026-03-26 | 0 | 0 | 0 | 0 |
| `docs/README.md` | 2026-03-05 | 0 | 0 | 0 | 0 |
| `promote.md` | 2026-06-14 | 0 | 0 | 0 | 0 |
| `IMPORT_EXPORT_VISION.md` | 2026-08-08 | 0 | 0 | 0 | 1 |

Les six premiers sont ceux qu'un visiteur ouvre en premier. **Aucun ne sait que
les trois applications existent** ; tous présentent Noge comme la seule
destination. `README.md` a été modifié le **12 août** sans que la lacune soit
comblée — ce n'est donc pas un simple retard de mise à jour, c'est un angle mort.

### 1.5 Ce qui manque et que personne n'a écrit

**Aucun fichier** ne décrit le protocole d'échange **par fichiers** entre les
trois applications. Recherche effectuée sur `communiquent par fichier`,
`communication par fichier`, `par fichiers interposés`, `échange par fichier` :
**zéro occurrence**. La moitié opérante de la décision 1 — *comment* elles
communiquent — n'est écrite nulle part.

---

## Décision 2 — NkAnima joue, NkAnimaEditor fabrique, le séquenceur appartient à NkAnima

### 2.1 NkAnima est rangé et décrit comme une application

| fichier | ligne | ce qui est affirmé |
|---|---|---|
| `Applications/NkAnima/ROADMAP.md` | 1 | « # NkAnima — Roadmap (**outil** d'animation physiquement correct + IA) » |
| `Applications/NkAnima/ROADMAP.md` | 5 | « Nom de travail **NkAnima** (à valider) » |

Le fichier est rangé sous `Applications/`, alors que NkAnima est la
**bibliothèque**. Mesure : `Applications/NkAnima/` ne contient **que ce
fichier** — aucun `.jenga`, aucun `.cpp`. Le code décrit vit ailleurs
(`Kernel/Runtime/NKAnimation/`). Le ROADMAP le reconnaît lui-même ligne 18 :
« `Applications/NkAnima/` ne contient à ce jour que ce fichier ».

C'est un document de **599 lignes**, modifié le **2026-08-21**, présenté comme
« à LIRE au démarrage d'une session NkAnima » : c'est le point d'entrée du sujet,
et il installe la confusion bibliothèque/application dès sa première ligne.

### 2.2 Les sémantiques de cinéma sont dans NkAnimaEditor

| fichier | ligne | ce qui est affirmé |
|---|---|---|
| `Applications/NkAnimaEditor/important/interface.md` | 1300 | « Interface dédiée à la gestion des caméras, à la **cinématographie virtuelle** et aux mouvements de caméra **pour le cinéma**. » |
| `Applications/NkAnimaEditor/important/interface.md` | 1337 | « ### Séquenceur de Coupes (**Cut Manager**) » |

Ce sont exactement les deux éléments que la décision 2 envoie à **NKScena**.
Ils sont ici dans le document d'interface de NkAnimaEditor (2 066 lignes,
2026-06-29). C'est aussi le **seul** fichier du dépôt où apparaissent
« Cut Manager » et « cinématographie virtuelle ».

### 2.3 Le séquenceur est documenté ailleurs que dans NkAnima

La décision dit : *le séquenceur appartient à NkAnima*. Les documents le placent
dans **Noge** et dans **Nogee**.

| fichier | ligne | ce qui est affirmé |
|---|---|---|
| `Engine/Noge/ROADMAP.md` | 1767 | « **Sequencer/Cinématiques** \| `Engine/Noge/src/Noge/Sequencer/NkSequencer.h` = spec seule » |
| `Engine/Noge/ROADMAP.md` | 3194 | item de phase A : « **`Sequencer/NkSequencer.h`** — structures de données/évaluation de… » |
| `wiki/Engine/Noge/Animation.md` | 140-144 | « ## Le séquenceur : orchestrer une cinématique […] `NkSequencer.h` en fournit **le modèle complet** » |
| `wiki/Engine/Noge/Animation.md` | 286 | « ### Séquenceur — `Noge/Sequencer/NkSequencer.h` » |
| `wiki/Engine/Noge.md` | 80-81, 108 | « **Animation** (`…/Sequencer/…`) — […] séquenceur multi-piste » |
| `wiki/Engine/Noge/README.md` | 22 | table d'entrée : « séquenceur » -> `Sequencer/NkSequencer.h` |
| `Applications/Nogee/design/01-specification-humaine.md` | 26, 159, 176, 385 | « 14. Sequencer / Timeline d'animation » ; « Icône Cinématique (ouvre Sequencer) » |
| `Applications/Nogee/design/02-specification-claude.md` | 132, 378, 407 | arborescence `Sequencer/` et panneau à construire |
| `Applications/Nogee/design/03-specification-banani.md` | 263-268 | « ## 9. Sequencer — Timeline d'animation » |

⚠️ **Ce que je ne peux pas trancher.** Noge est un moteur de jeu construit
au-dessus de Nkentseu ; il n'est pas absurde qu'il expose sa propre timeline.
Mais `wiki/Engine/Noge/Animation.md:144` écrit que `NkSequencer.h` « en fournit
le **modèle complet** » — c'est-à-dire *le* modèle, pas une façade. Si le
séquenceur appartient à NkAnima, alors soit ces neuf documents désignent un
doublon à fusionner, soit la décision doit dire explicitement que Noge en
consomme une façade. **La question doit être tranchée par Rodolf**, pas par moi ;
je ne fais que signaler que neuf documents répartis sur trois zones du dépôt
(Engine, wiki, Applications) placent aujourd'hui le séquenceur hors de NkAnima.

### 2.4 Le fichier vide

`Applications/NkAnimaEditor/important/roadmap.md` fait **0 octet**. Il est suivi
par git depuis le 2026-06-29. Un fichier vide nommé `roadmap.md` dans le dossier
`important/` d'une application est pire qu'un fichier absent : il répond
« déjà fait » à qui cherche s'il existe une roadmap.

---

## Décision 3 — le nodal vient PAR-DESSUS le non nodal

**Énoncé.** Le graphe **compile vers** la représentation non nodale ; il ne la
remplace pas. Tout document qui présente un graphe comme **le** modèle est faux.

### 3.1 Le document qui inverse le sens

| fichier | ligne | ce qui est affirmé |
|---|---|---|
| `Applications/NKUIDesign/2_NkUIDesign_Langage_Description_NodeBlueprint.md` | 232-234 | « Au-delà, la vue **Node Graph reste la source de vérité** (plus expressive) et la vue Code devient une **lecture seule générée** (pretty-print) » |

Le paragraphe qui précède (§6.4, l. 222-231) est pourtant juste : les deux fronts
« compilent vers la **même représentation intermédiaire** ». Puis, dès que le
programme sort d'un sous-ensemble borné, le document bascule et fait du graphe la
source de vérité, le texte devenant une sortie. C'est exactement l'inversion que
la décision 3 interdit.

⚠️ **Nuance à garder.** La phrase parle de l'aller-retour Code <-> Graphe *à
l'intérieur du langage `.nkgui`*, pas de la représentation non nodale d'une
scène 3D. Elle n'en reste pas moins la seule phrase du dépôt qui accorde à un
graphe le statut de source de vérité. Fichier de 343 lignes, modifié le
2026-07-17.

### 3.2 Ce qui est conforme, et qu'il ne faut pas casser en corrigeant

À vérifier avant toute retouche, parce que ces documents disent **déjà** la bonne
chose et sont la meilleure formulation existante de la décision 3 :

- `Kernel/Runtime/NKGraph/ROADMAP.md` l. 13-35 — architecture en 3 couches, le
  cœur ne connaît aucune sémantique, « les matériaux se **compilent** vers NkSL ».
- `Applications/NK3DModeler/PASSATION_2026_08_08.md` l. 116 — « `.nkmat` couvre le
  matériau **simple ET nodal** : le nodal est une façon de… ».
- `Applications/NK3DModeler/ROADMAP.md` l. 99 — « le nodal viendra avec NKGraphe ».

---

## Décision 4 — l'organisation GitHub est `Rihen-Universe`, avec un tiret

**Mesure.** `RihenUniverse` sans tiret : **5 lignes dans 4 fichiers**.
`Rihen-Universe` avec tiret : **26 lignes dans 7 fichiers**. La forme correcte
est déjà majoritaire ; le résidu est petit et localisé.

| fichier | ligne | ce qui est affirmé | classement |
|---|---|---|---|
| `docs/SESSION_FINAL_REPORT_2026.md` | 572 | « Repository: **RihenUniverse/Jenga** (main branch) » | **contredit la décision 4** — désigne bien l'organisation |
| `Applications/NKCode/important/i1.md` | 1179 | maquette ASCII : « GitHub: [Connecté: **@RihenUniverse**] » | **contredit** — maquette d'interface qui affichera le mauvais handle si elle sert de référence |
| `BugReports/GitHub/claude-contributeur-cache-github.md` | 11, 16 | « à côté de `LeTeguis` et `RihenUniverse` » | ⚠️ **pas sûr que ce soit une erreur** : le contexte est la liste des **contributeurs**, donc des comptes utilisateur, pas l'organisation. Un compte personnel `RihenUniverse` peut coexister avec l'org `Rihen-Universe`. **À confirmer par Rodolf.** |
| `docs/ssh.md` | 118 | « Hi **RihenUniverse**! You've successfully authenticated… » | ⚠️ **pas sûr** : c'est la sortie de `ssh -T git@github.com`, qui affiche le **compte** authentifié. Si le compte s'appelle bien ainsi, la phrase est exacte. **À confirmer.** |

**À retenir** : sur cinq occurrences, **deux seulement sont certainement fausses**.
Un `grep` seul aurait rendu « 5 erreurs » — un chiffre faux qui a l'air juste.

---

## Décision 5 — l'entreprise est « Rihen » ; « Rihen Universe » ne désigne que les chaînes

**Mesure.** « Rihen Universe » (en toutes lettres, avec espace) : **4 lignes dans
4 fichiers** sur 747.

| fichier | ligne | ce qui est affirmé | classement |
|---|---|---|---|
| `Applications/Pong/README.md` | 125 | « **Éditeur : Rihen Universe**. » | **contredit** — l'éditeur d'un logiciel est l'entreprise : doit être « Rihen » |
| `Applications/Pong/ROADMAP.md` | 108 | « Métadonnées installeur (**publisher Rihen Universe**, licence GPL3…) » | **contredit** — même motif, et celui-là finit dans les métadonnées d'un binaire livré |
| `Documentation/cours/md/04-application.md` | 1033 | exemple de code : `apppublisher("Rihen Universe")` | **contredit**, et c'est le plus coûteux : c'est un **cours**, donc du texte copié tel quel par des étudiants |
| `Applications/NkAnimaEditor/important/f1.md` | 940 | maquette : « Connecté: 👤 Rihen Universe   Studio: Rihen Studio » | ⚠️ **probablement légitime** : c'est un **nom de compte** affiché dans une maquette, donc l'usage « chaîne/compte ». Signalé sans le classer faux. |

---

## Ce que je n'ai pas pu vérifier

1. **Les fichiers non suivis par git sont hors périmètre.** `git grep` ne voit que
   les 747 fichiers suivis. Un balayage disque remonte par exemple
   `Build/_jenga-embed/Jenga/Unitest/Readme.md` — non suivi, donc non compté, non
   analysé. Le chiffre 747 est un chiffre *de dépôt*, pas *de disque*.
2. **Je n'ai pas vérifié le code.** Toutes les affirmations ci-dessus sont des
   divergences **entre documents et décisions énoncées**. Je n'ai pas ouvert les
   `.h`/`.cpp` pour savoir laquelle des deux versions correspond au code réel.
   Exemple ouvert : `NkSequencer.h` existe-t-il vraiment sous `Engine/Noge/` et
   qu'y a-t-il dedans ? Le ROADMAP de Noge dit « spec seule, 0 `.cpp` » ; je ne
   l'ai pas vérifié moi-même.
3. **La décision 2 n'est pas assez précise pour trancher le cas Noge.** Voir §2.3.
4. **Je n'ai pas relu les 747 fichiers.** Les divergences listées viennent de
   recherches ciblées sur les termes des cinq décisions, suivies d'une relecture
   du contexte de chaque ligne retenue. Un document peut contredire une décision
   sans employer aucun de ces termes — ce balayage-là n'a pas été fait, et il ne
   se fait pas par `grep`.
