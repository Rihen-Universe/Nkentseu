# Cartographie des 747 markdown — mesures, divergences, carte de consolidation

> Arbre `D:/Projets/2026/Nkentseu/Nkentseu-merge`, branche `main`, base `9b31cc8f`.
> Produit le 2026-08-23. **Aucun document du dépôt n'a été modifié ni supprimé.**
> Les seuls fichiers créés sont ceux de `cartographie_markdown/`.
>
> Trois fichiers accompagnent ce rapport :
> - `inventaire.csv` — les 747, avec taille, lignes, date de dernière modification ;
> - `divergences.md` — tâche 1 : fichier, ligne, affirmation, décision contredite ;
> - `carte_consolidation.csv` — tâche 2 : une ligne par fichier, avec action,
>   cible de fusion, qui le lit, ce qu'il contient d'irremplaçable, date ;
> - `classer.py` — les règles qui ont produit la carte, lisibles et rejouables.

---

## 0. La mesure d'ampleur, refaite

| ce qui a été mesuré | valeur |
|---|---|
| markdown **suivis par git** sur `main` | **747** |
| volume total | **11,2 Mo**, **231 394 lignes** |
| markdown présents sur une autre branche et **absents de `main`** | **24** |
| total réel toutes branches locales confondues | **771** |

Répartition, et écart avec le chiffre annoncé au départ :

| zone | mesuré | annoncé | écart |
|---|---|---|---|
| `docs/` | 190 | 190 | — |
| `wiki/` | 187 | 187 | — |
| `Kernel/` | **128** | 131 | −3 |
| `Applications/` | 103 | 103 | — |
| `Documentation/` | 34 | 34 | — |
| `BugReports/` | 34 | *(dans « le reste »)* | — |
| `Spark/` | 23 | | |
| racine | 16 | | |
| `Guides/` | 10 | | |
| `Externals/` | 8 | | |
| `Engine/` | 7 | | |
| `Tutoriels3D/` | 6 | | |
| `Resources/` | 1 | | |

L'écart sur `Kernel/` (128 contre 131) vient probablement du comptage disque
contre le comptage git : un balayage du disque attrape des fichiers non suivis.
**Le 747 est un chiffre de dépôt, pas de disque.**

### L'âge, qui est le vrai diagnostic

| dernière modification | fichiers |
|---|---|
| mars 2026 | **181** |
| avril 2026 | 4 |
| mai 2026 | 12 |
| juin 2026 | 356 |
| juillet 2026 | 78 |
| août 2026 | 116 |

**197 fichiers n'ont pas bougé depuis mai.** Les décisions que ce chantier
vérifie — les trois applications, la séparation NkAnima / NkAnimaEditor, le nodal
par-dessus le non nodal — sont toutes postérieures. Ces 197 fichiers ne sont pas
« en retard » : ils décrivent une autre conception.

---

## 1. Les deux chiffres demandés

### 1.1 Combien pourraient disparaître sans perte mesurable : **172**

| zone | fichiers | motif |
|---|---|---|
| `docs/*/markdown/**` | **158** | documentation d'API **produite par un outil**, gelée les 5 et 10 mars 2026, pour six modules (NKCore 39, NKLogger 37, NKWindow 28, NKWindow_Tests 28, NKPlatform 15, Sandbox 11) |
| `docs/*.md` orphelins | 9 | `dd2.md`/`dd3.md`/`dd4.md` (trois versions successives du même document d'architecture, 6 183 lignes), `docs/README.md` (vieille copie du README racine), `docs/ARCHITECTURE.md`, et 4 rapports de session de mars |
| fichiers vides ou quasi vides | 3 | dont `Applications/NkAnimaEditor/important/roadmap.md` à **0 octet** |
| `CONSOLIDATION_TODO.md` | 1 | consigne périmée, voir §4 |
| `promote.md` | 1 | note de salon, événement de juin passé |

Soit **172 fichiers, 865 Ko, 29 713 lignes** — **23 % des fichiers pour 8 % du
volume**. Ce sont de petits fichiers : c'est le nombre qui coûte, pas la taille.

**La mesure qui justifie les 158 (et les 9).** J'ai cherché les **liens
entrants** : `git grep` de `](docs/`, `(./docs/`, `../docs/` dans tous les
markdown suivis **hors de `docs/`**. Résultat : **zéro lien** vers la racine
`docs/`. Les onze occurrences trouvées désignent toutes un dossier `docs/`
**local à une application** (`Applications/Pong/docs/`, `Applications/Mou/docs/`).
Le `README.md` de la racine ne cite jamais `docs/`. **Personne ne peut arriver
sur ces fichiers en naviguant.**

Et ils font double emploi : les quatre modules que `docs/` documente sur
**119 fichiers générés** sont déjà couverts par `wiki/` en **22 fichiers écrits
à la main**.

| module | `wiki/` | `docs/…/markdown/` |
|---|---|---|
| NKCore | 9 | 39 |
| NKLogger | 4 | 37 |
| NKWindow | 4 | 28 |
| NKPlatform | 5 | 15 |

⚠️ **Le seul risque, et il est réel.** Ces fichiers ont l'air générés, mais
**aucun générateur n'est suivi par git** (pas de `Doxyfile`, pas de script, hors
`Externals/Libs/pybind11/docs/Doxyfile` qui appartient à une dépendance tierce).
Je n'ai donc **pas vérifié** qu'on sait les régénérer. Si l'outil est perdu, on
supprime un artefact non reproductible — sans valeur d'usage, mais non
reproductible. **Recommandation : retrouver ou réécrire le générateur avant de
supprimer, ou supprimer en assumant qu'on ne les régénérera pas** (ce que je
recommande : ils décrivent l'API de mars).

### 1.2 Combien portent une mesure ou une décision introuvable ailleurs : **134**

Ceux-là **ne se suppriment pas**. Vérification faite : **aucun des 172 à
supprimer n'est classé irremplaçable**, et **4 des 134 sont en « fusionner »** —
c'est-à-dire que leur contenu doit migrer avant que le fichier disparaisse.

| famille | nombre | pourquoi c'est irremplaçable | vers où si on fusionne |
|---|---|---|---|
| `BugReports/**` | **31** | symptôme + cause + **leçon**, écrits le jour où le bug a coûté la journée. Six dossiers : Backends-Rendu (9), DirectX12 (7), NKCanvas (6), NkSL-Générateurs (3), DirectX11, NkSL-VM, Mémoire-Heap, Pong, Environnement-Windows | **rester où ils sont** — ils sont déjà bien rangés, par backend |
| `Kernel/**/ROADMAP.md` | **63** | état réel daté, mesures de perf, décisions avec leur raison. Ce sont eux qui portent le POURQUOI, faute de `SPECIFICATION.md` (voir §3) | rester, mais à scinder pour les plus gros |
| `Applications/**` | **22** | passations (`PASSATION_2026_08_08.md`), documents de design de jeu (`*-gdd.md` de Nkoung et Mou), notices de témoins (`LISEZMOI.md`) | rester |
| `Spark/**` | **11** | ROADMAP des 10 modules du compilateur | rester |
| `docs/` | 5 | `docs/recherche/*/SOURCE.md` (bibliographie sourcée), `Audit-Portabilite-Apple.md`, `LANGUES_LOCALES_CAMEROUN.md` | `NKPlatform`, et garder pour la recherche |
| `Engine/` | 2 | ROADMAP de Noge et de NKEditorKit | rester |

**93 fichiers supplémentaires sont marqués « à relire avant de décider »** dans
`carte_consolidation.csv`. Je ne les ai pas ouverts. Les classer sur leur nom
serait exactement l'erreur que ce chantier doit éviter.

### 1.3 Où on atterrit

| | fichiers | volume |
|---|---|---|
| **garder** | **440** | 7 509 Ko |
| **fusionner** (le fichier disparaît, son contenu monte) | **135** | 2 542 Ko |
| **supprimer** | **172** | 865 Ko |

**747 → 440 fichiers, soit −41 %.** Et l'essentiel du gain vient d'une seule
zone : `docs/` passe de **190 à 6**.

---

## 2. ⚠️ La découverte qui change l'ordre des opérations

**Les fichiers qui portent les mesures les plus coûteuses ne sont pas sur `main`.**

24 markdown existent sur une branche locale et sont absents de `main`. Parmi eux,
exactement ceux que ce chantier devait protéger :

| fichier | branche | ce que c'est |
|---|---|---|
| `VERIFICATEUR.md` | `feat/verificateur` | le fichier nommé dans la commande de mission comme portant des mesures coûteuses. **Il n'existe ni dans `main`, ni sur le disque de cet arbre.** |
| `CAPACITES_A_DATER.md` | `feat/verificateur` | 24 notes à dater, d'après les messages de commit |
| `Documentation/RenduTemoins/README.md` | `feat/rendu-temps-reel` | les témoins de rendu |
| `Kernel/Runtime/NKGraph/tests/LISEZMOI.md` | `feat/design-nodal` | notice de témoins |
| `Kernel/Runtime/NKGraph/references/generateurs/LISEZMOI.md` | `feat/design-nodal` | notice de témoins |
| `Kernel/Runtime/NKGraph/SPECIFICATION_VISUELLE.md` | `feat/design-nodal` | spécification visuelle de l'éditeur nodal — le fichier est **sur le disque de cet arbre mais non suivi par git sur `main`** |
| `Applications/NKUIDesign/design/planches/LISEZMOI.md` | branche NKUIDesign | notice de planches |
| 14 `carnets/**/*.private.md` | diverses | carnets de travail privés |

Les messages de commit de `feat/verificateur` sont eux-mêmes des mesures :
*« Passe complet du 23/08 : 12 bancs, 10 OK, 2 ÉCHEC, 0 IGNORÉ »*,
*« GetCaps() n'est pas appelé 45 fois, mais 29 »*.

**Conséquence pratique : ne pas consolider `main` avant d'avoir fusionné ces
branches**, sinon on consolide autour du mauvais ensemble et on réintroduit
les fichiers manquants après coup, sous d'autres noms. C'est aussi la preuve
vivante du problème qu'il faut empêcher : *une leçon écrite mais introuvable
se repaie.* Ici elle est écrite, versionnée, et introuvable depuis `main`.

---

## 3. Critique de la cible proposée

> `README.md` (celui qui utilise) · `SPECIFICATION.md` (celui qui maintient) ·
> `ROADMAP.md` (celui qui décide) · plus `Documentation/` pour ce qui se lit
> comme un livre.

**La forme est juste. Trois objections, toutes chiffrées.**

### 3.1 Le `Kernel/` respecte déjà la cible — ce n'est pas là qu'est le problème

| | mesuré |
|---|---|
| modules `Kernel/` (dossiers avec un `.jenga`) | **48** |
| markdown dans ces dossiers | **88** — soit **1,8 par module** |
| modules avec un `README` | 30 |
| modules avec un `ROADMAP` | 43 |
| modules avec **aucun** markdown | 3 (`NKGpt`, `NKAnimPhysics`, `NKNavigation`) |
| modules avec **plus de 2** markdown | **6** seulement |

Et la médiane des 125 `README` du dépôt est de **34 lignes** — la cible « ~50
lignes » est déjà tenue. Seuls 20 dépassent 200 lignes.

**Appliquer la cible au `Kernel/` ne rendrait presque rien.** Le désordre est
ailleurs : `docs/` (190 fichiers, 0 module), `Applications/` (103 fichiers
concentrés sur 19 applications seulement — NKCode 18, Nkoung 17, NK3DModeler 9,
NKUIDesign 9, Mou 9), et `wiki/` (187).

### 3.2 ⚠️ `SPECIFICATION.md` n'existe nulle part, et le créer en masse est le piège

**Mesure : 0 module sur 48 n'a de `SPECIFICATION.md`.** Le « pourquoi » vit
aujourd'hui **dans les ROADMAP** — et c'est exactement pour ça qu'ils sont
énormes :

| taille des 86 `ROADMAP.md` | nombre |
|---|---|
| < 200 lignes | 49 |
| 200–500 | 24 |
| 500–1 000 | 5 |
| **> 1 000** | **8** |

`NK3DModeler/ROADMAP.md` 3 481 lignes · `Engine/Noge/ROADMAP.md` 3 343 ·
`NKIlyana/ROADMAP.md` 2 277 · `NKRenderer/ROADMAP.md` 1 998.

⚠️ **Décider « chaque module aura une SPECIFICATION.md » revient à commander
48 fichiers neufs que personne n'a relus** — précisément les « 742 fichiers que
personne ne peut vérifier » que la mission interdit. Le scindage d'un ROADMAP
de 3 481 lignes en README + SPECIFICATION + ROADMAP est un travail de lecture,
pas un travail de gabarit.

**Proposition** : `SPECIFICATION.md` **n'est pas obligatoire**. Elle se crée
**quand le ROADMAP dépasse 500 lignes** — c'est-à-dire pour **13 modules sur 48**,
pas 48. Les 49 ROADMAP de moins de 200 lignes se suffisent à eux-mêmes ;
leur imposer deux fichiers de plus triplerait leur nombre pour rien.

### 3.3 La cible n'a pas de case pour trois familles qui existent et sont légitimes

| famille | combien | pourquoi elle ne rentre dans aucune des trois |
|---|---|---|
| **rapport de bug** | 31 | ce n'est ni un mode d'emploi, ni une conception, ni un plan : c'est **le récit daté d'une erreur**. Le mettre dans `SPECIFICATION.md` le noie ; le supprimer fait repayer la journée |
| **passation / état à une date** | ~6 | `PASSATION_2026_08_08.md` est **volontairement figé** : sa valeur vient de ce qu'il ne bouge pas. Un ROADMAP, lui, doit bouger |
| **notice de témoin** | ~5 (dont 4 hors de `main`) | dit **ce qui a été mesuré, comment, avec quel banc**. Sans elle, un témoin est une image sans protocole |

**Proposition** : ajouter une quatrième case explicite — un dossier
`JOURNAL/` (ou garder `BugReports/`, qui fonctionne déjà bien) pour tout
document **daté et immuable**. Règle simple qui les distingue : *les trois
fichiers cibles se mettent à jour ; le quatrième s'ajoute et ne se modifie
jamais.*

### 3.4 La question `wiki/` n'est pas tranchée par la cible et pèse 187 fichiers

`wiki/README.md` annonce viser « une description **exhaustive et exacte** de
chaque module : chaque classe, fonction et type publics ». C'est le même travail
que la documentation d'API, en plus coûteux car écrit à la main. Mesure :
**180 des 187 pages n'ont pas bougé depuis juin 2026**, pour 3,9 Mo.

Je les classe **garder**, parce qu'elles sont publiées et lues de l'extérieur —
c'est le tri qui compte, et un document lu ne se supprime pas à la légère. Mais
**la vraie question à trancher n'est pas « garder ou supprimer », c'est « qui
tient à jour 187 pages d'API à la main ? ».** Si la réponse est « personne », le
wiki deviendra la prochaine `docs/`. **C'est la décision que je recommande de
prendre en premier**, avant toute suppression.

---

## 4. Deux exemples concrets, à titre de démonstration

**`CONSOLIDATION_TODO.md`** (racine, 48 lignes, 2026-06-29) dit en gras :
*« Un `git merge` brut dupliquerait NKCode → conflits massifs. **NE PAS faire un
merge brut.** »* et liste 9 commits à rejouer. Vérification : `NKCollision`
existe sur `main` (20 fichiers), `SetFinalColorTarget` est utilisé dans
`NK3DModeler` — **le contenu a atterri**. Le fichier donne aujourd'hui une
consigne fausse, à la racine, là où un agent qui reprend regarde en premier.
*(Nuance : j'ai vérifié que le travail est arrivé, pas que chacun des 9 commits
a été rejoué — les hachages d'origine ne sont pas ancêtres de `HEAD`, ce qui est
normal après un cherry-pick.)*

**`Applications/Nkoung/`** — 17 markdown pour une application, dont
`ARCHITECTURE.md` **et** `ARCHITECTURE_NKOUNG.md`, plus `COMPLETION_SUMMARY.md`,
`FILES_DELIVERED.md`, `INDEX.md`, `QUICK_START.md`, `WELCOME.md`. Sept fichiers
de méta-livraison autour d'un `README.md` qui existe déjà. Les six documents de
design de jeu (`docs/0X-*-gdd.md`) sont, eux, irremplaçables. C'est le motif
exact de la repousse : **le contenu utile et le bruit poussent au même endroit,
le jour de la livraison.**

---

## 5. Le mécanisme qui empêche la repousse

### 5.1 Critique de la piste proposée

> *Un nouveau markdown porte la raison de son existence dans son en-tête,
> vérifiable mécaniquement.*

Trois faiblesses, et elles sont sérieuses :

1. **Ce qui est vérifiable mécaniquement, c'est la présence de l'en-tête, pas la
   vérité de la raison.** Celui qui produit 742 fichiers produit 742 en-têtes.
   `COMPLETION_SUMMARY.md` écrira sans mentir : *« raison : récapituler ce qui a
   été livré aujourd'hui »*. La règle est satisfaite et le fichier ne devait pas
   exister. **Une contrainte qu'on satisfait sans changer de comportement ne
   contraint pas.**
2. **Elle ne s'applique qu'aux nouveaux.** Les 747 existants n'ont pas d'en-tête :
   la règle leur accorde une amnistie de fait, alors que ce sont eux le problème.
3. **Elle ne dit rien sur l'endroit.** La repousse mesurée n'est pas « un fichier
   sans raison » : c'est **un septième fichier à côté d'un README qui existe
   déjà**. La raison est individuellement défendable ; c'est l'ensemble qui est
   faux. Une règle par fichier ne peut pas voir un défaut d'ensemble.

**Ce qu'elle a de bon et qu'il faut garder** : elle est *mécanique*. Une règle
qu'un humain doit juger ne survit pas à un mois chargé.

### 5.2 Ce que je propose à la place — trois vérifications, toutes binaires

**(A) Une liste blanche de noms par type de dossier.** Dans un dossier qui
contient un `.jenga`, les seuls noms admis sont `README.md`, `SPECIFICATION.md`,
`ROADMAP.md`. Toute exception vit dans **un seul fichier** `docs-exceptions.txt`
à la racine, une ligne par exception : `chemin — raison — qui l'a décidé — date`.

C'est infalsifiable : le nom du fichier ne se négocie pas. Et ça déplace la
raison d'écriture **du fichier vers un endroit relu** — au lieu de 747 en-têtes
que personne ne lit, une liste courte qui saute aux yeux quand elle grossit. La
longueur de `docs-exceptions.txt` **est** l'indicateur de dette.

**(B) Une liste noire de noms, par expression régulière sur le nom de fichier.**
`COMPLETION`, `SUMMARY`, `FINAL_REPORT`, `SESSION_`, `DELIVERED`, `STATUS`,
`WELCOME`, `INDEX`, `QUICK_START`, `dd[0-9]`. Mesure à l'appui : **ce motif
seul attrape 6 des 32 fichiers non générés de `docs/` et 5 des 17 de `Nkoung/`**
— de la méta-livraison, à chaque fois. *(Il en laisse passer :
`FINAL_INTEGRATION_REPORT_2026.md` échappe au motif `FINAL_REPORT`. Élargir à
`REPORT` attraperait aussi des rapports légitimes — la liste noire doit rester
étroite et faillible, la liste blanche (A) est ce qui ferme vraiment.)* Ce sont les fichiers qu'on écrit *pour clore une
session*, jamais pour être lus. Coût de la règle : une ligne de regex.

**(C) Une date de péremption sur tout document qui énonce un état.** Un document
qui contient « ✅ », « terminé », « % complet » ou un tableau d'état doit porter
en en-tête `> Mesuré le AAAA-MM-JJ`. La vérification échoue si la date dépasse
90 jours. C'est *cette* règle-là qui attrape le défaut réellement mesuré ici :
**197 fichiers sans modification depuis mai**, et un `docs/COMPLETION_STATUS.md`
figé au 5 mars qui annonce encore un statut comme s'il était vrai.

Contrairement à un en-tête de « raison », une date **se périme toute seule** :
personne n'a besoin de juger, le calendrier juge.

**(D) La condition sans laquelle rien ne tient : la vérification s'exécute au
moment où le document est produit**, en *hook* (`PostToolUse` / `Stop`) et en CI,
pas dans un ménage mensuel. Un ménage mensuel, c'est la dette qu'on a
aujourd'hui, avec une réunion en plus.

**(E) Et une règle qui ne coûte rien, tirée du §2 :** *un document qui porte une
mesure doit arriver sur `main`.* Aujourd'hui `VERIFICATEUR.md`,
`CAPACITES_A_DATER.md` et quatre notices de témoins vivent sur des branches non
fusionnées. Une vérification qui liste les markdown présents sur une branche et
absents de `main` depuis plus de N jours coûte trois lignes de script — et
c'est celle qui aurait évité de payer trois fois la même leçon cette semaine.

### 5.3 L'ordre que je recommande

1. **Fusionner les branches qui portent les mesures** (`feat/verificateur`,
   `feat/design-nodal`, `feat/rendu-temps-reel`) — §2. Rien ne se supprime avant.
2. **Trancher la question `wiki/`** — §3.4. C'est 187 fichiers, un quart du total.
3. **Supprimer les 172** — §1.1. Le seul lot sans discussion, une fois la
   question du générateur d'API tranchée.
4. **Corriger les divergences de `divergences.md`** — c'est du texte à retirer ou
   à réécrire, pas du texte à produire : peu de lignes, gros effet.
5. **Créer `Applications/NKScena/`** — la troisième application n'a pas une ligne,
   et écrire le protocole d'échange **par fichiers** entre les trois, qui n'existe
   nulle part (§1.5 de `divergences.md`).
6. **Poser les vérifications (A)(B)(C)(D)(E)** — avant de traiter les 135
   fusions, sinon elles repoussent pendant qu'on les traite.
7. **Traiter les 135 fusions**, module par module, en commençant par les 13
   ROADMAP de plus de 500 lignes.

---

## 6. Ce que je n'ai pas pu vérifier — à lire aussi fort que le reste

1. **Je n'ai pas lu les 747 fichiers.** J'ai lu intégralement une vingtaine de
   fichiers, l'en-tête ou une section d'une soixantaine d'autres, et classé le
   reste **par règles explicites** (`classer.py`, lisible et rejouable) fondées
   sur le chemin, le nom, la taille et la date. **93 fichiers sont marqués
   « à relire avant de décider »** dans la carte : ce n'est pas une échappatoire,
   c'est le périmètre honnête de ce qu'une passe de deux heures peut établir.
2. **Le classement « qui le lit » est un raisonnement, pas une mesure.** Sauf
   pour `docs/`, où il est mesuré (zéro lien entrant). Ailleurs — `wiki/`,
   `Documentation/cours/` — je déduis le lecteur de la nature du document. Je
   n'ai aucune donnée de fréquentation. **Si des statistiques de consultation du
   wiki existent, elles vaudraient mieux que tout ce rapport sur ce point précis.**
3. **Je n'ai pas confronté les documents au code.** Les divergences listées sont
   entre *documents* et *décisions énoncées*. Quand deux documents se
   contredisent, je n'ai pas ouvert les `.h`/`.cpp` pour dire lequel a raison.
4. **Le générateur des 158 fichiers d'API est introuvable** (§1.1). C'est le seul
   endroit où je recommande une suppression sans pouvoir garantir la
   réversibilité.
5. **Les fichiers non suivis par git sont hors périmètre.** Tout passe par
   `git grep` et `git ls-files`.
6. **La branche `feat/verificateur` n'a pas été inspectée**, seulement détectée.
   Son `VERIFICATEUR.md` peut contredire ou compléter ce rapport ; je ne l'ai pas
   ouvert.
7. **Je n'ai pas vérifié que les 24 fichiers hors `main` sont absents pour de
   mauvaises raisons.** Les 14 `carnets/*.private.md` sont sans doute privés
   volontairement. Les autres — `VERIFICATEUR.md`, les `LISEZMOI` de témoins —
   ont l'air d'être en attente de fusion, pas exclus par choix. **À confirmer par
   Rodolf.**
