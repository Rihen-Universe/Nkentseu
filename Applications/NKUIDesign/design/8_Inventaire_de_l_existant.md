# Document 8 — Inventaire de l'existant

> Écrit le **2026-08-21 vers 5 h**, après que Rodolf a rappelé que NKGui porte déjà
> NKCode, NK3DModeler et Mou.
>
> ⚠️ **Ce document existe parce que j'ai spécifié six sections d'un éditeur sans
> avoir lu les fondations sur lesquelles il repose.** Il rattrape cela.

---

## 1. Ce qui a déclenché cet inventaire

En vérifiant une règle que j'avais écrite la veille — *« l'unicité des raccourcis se
vérifie au démarrage, parce que les collisions n'existent nulle part tant que les
raccourcis ne sont pas rassemblés dans une seule table »* — j'ai ouvert
`NkShortcutTable.h`.

Il porte, écrit avant moi :

> *« les CONFLITS sont invisibles. Cas réel : `Shift+S` était déjà pris par
> « ombrage smooth »… découvert à la compilation, par hasard, alors qu'une table
> l'aurait dit immédiatement. »*

**Même problème, même raisonnement, même conclusion — déjà implémenté.** Et ce
n'était pas la première fois de la nuit : l'infobulle non-nœud, les noms
`PascalCase`, l'animateur de NKUI. **Quatre fois.**

> **Une spécification écrite sans lire l'existant n'est pas seulement redondante :
> elle peut le CONTREDIRE, et personne ne s'en apercevra avant que quelqu'un
> construise à partir d'elle.**

---

## 2. La pile réelle

| module | lignes | rôle |
|---|---|---|
| **NKGui** | 8 096 | widgets, contexte, drawlist, police, entrée, **thème** |
| **NKEditorKit** | 7 451 | la coquille d'éditeur : docking, panneaux, modales, commandes |
| *(NKUI)* | *22 430* | **déprécié — ne porte aucune application** |

**Applications réellement portées par NKGui + NKEditorKit** : `NKCode`,
`NK3DModeler`, `Mou`, `NKUIDesign`. *(`Pong` est sur NKCanvas, pas sur NKGui.)*

⚠️ **Le volume ne mesure pas la capacité.** J'avais écrit que NKGui « manquait
14 000 lignes » face à NKUI. La pile réelle fait 15 547 lignes et porte quatre
applications ; NKUI en fait 22 430 et n'en porte aucune.

---

## 3. Ce qui existe déjà dans NKEditorKit

| fichier | lignes | ce qu'il fait | ce que j'avais spécifié comme à faire |
|---|---|---|---|
| `NkEditorShell` | 599 + **2 652** | fenêtre, **docking**, panneaux | §13 rails et panneaux |
| `NkFilePicker` | 892 | sélecteur de fichiers/dossiers modal | §14ter.6 |
| `NkEditorModal` | 427 | dialogue modal déplaçable | §20bis dialogues de greffon |
| `NkShortcutTable` | 358 | **table de raccourcis configurable, conflits détectés** | §1nonies.2 `E-SHORTCUT-DUPLICATE` |
| `NkFontPrefs` | 341 | préférences de police | §20 |
| `NkTheme` | 302 | **rôles de couleur nommés, héritage, chargement** | §2 système de thème |
| `NkEditorTextField` | 296 | champ de saisie réutilisable | — |
| `NkEditorContextMenu` | 255 | **menu contextuel réutilisable** | §5bis.11 |
| `NkEditorScrollbar` | 175 | barre de défilement standard | — |
| `NkEditorCombo` | 147 | menu déroulant | — |
| `NkEditorInspector` | 141 | **inspecteur générique piloté par NKReflection** | §12.2 |
| `NkDirBrowser` | 126 | navigation de dossiers | — |
| `NkEditorTooltip` | 54 | **infobulle avec DÉLAI de survol** | §14quinquies.3 |
| `NkEditorCommand` | 31 | commande nommée + registre | §1nonies.1 |

> **La coquille d'éditeur n'est pas à construire. Elle tourne, et trois
> applications s'en servent.**

### ⚠️ 3.1 « Ça existe » ne veut pas dire « c'est figé »

*Précision de Rodolf, 2026-08-21 : ces systèmes doivent pouvoir évoluer et
l'utilisateur doit pouvoir les modifier.*

Ma conclusion penchait vers « déjà fait, ne pas y toucher ». **C'est la mauvaise
lecture.** Le bon usage de l'inventaire est : *on bâtit dessus, et on veille à ce
que ça reste ouvert.*

Et ces cinq-là sont exactement les **points d'extension** que §20bis.1 attend :

| existant | ce qu'un greffon y ajoute | ce que l'utilisateur y change |
|---|---|---|
| `NkShortcutTable` | ses commandes et leurs liaisons par défaut | **toutes les liaisons** |
| `NkEditorCommand` | ses commandes nommées | l'ordre, les favoris |
| `NkEditorInspector` | des éditeurs pour ses propres types | l'ordre et le repli des sections |
| `NkTheme` | un thème livré | **chaque rôle de couleur** |
| `NkEditorContextMenu` | des entrées contextuelles | ce qui s'affiche |

`NkShortcutTable` porte déjà la moitié du travail dans son intention écrite :
*« la liaison touche → commande vit en donnée, jamais en dur »*, et « la
reconfiguration, sans recompilation ». **Ce qui reste, c'est de tenir la même
promesse pour les quatre autres.**

---

## 4. ⚠️ Deux contradictions à trancher, pas à ignorer

### 4.1 L'Inspecteur : sections normatives contre grille réflexive

`NkEditorInspector` est **générique, piloté par NKReflection** — une `PropertyGrid`
qui affiche ce que le type déclare.

§12.2 spécifie l'inverse : **dix sections dans un ordre normatif**, avec des règles
sur ce qui se grise et ce qui disparaît.

**Ce sont deux architectures différentes.** Une grille réflexive affiche les
propriétés dans l'ordre où le type les déclare ; ma spécification impose un ordre
qui ne dépend pas du type. Les deux peuvent coexister — l'un pour l'inspecteur
d'objet, l'autre pour l'inspecteur de conception — **mais il faut le dire**, sinon
celui qui implémentera choisira au hasard.

### 4.2 L'infobulle : deux implémentations possibles pour un même mot

`NkEditorTooltip` fait l'infobulle **de l'éditeur** (§14quinquies, ligne 1 du
tableau des trois notions). Elle ne fait pas l'infobulle **conçue**, celle qui part
dans le document exporté.

⚠️ **Il ne faut pas les confondre au moment d'implémenter**, sous prétexte
qu'« une infobulle existe déjà ». La première ne se sérialise pas ; la seconde si.

---

## 5. Ce qui manque réellement à NkUIDesign

| manque | ampleur | bloque quoi |
|---|---|---|
| **parseur / sérialiseur `.nkgui`** | grammaire écrite, code inexistant | **tout** — l'éditeur ne peut ni enregistrer ni relire |
| rôle `Text`, `Spacer` dans le format | table à compléter | la conversion, le corpus |
| apparence par widget | décision + grammaire | l'export de ce qu'on dessine |
| section d'animation dans le format | grammaire + sérialisation | §9ter en entier |
| sémantique du canvas de conception | ancres, bornes, modes de taille | §8quater |
| animation à l'exécution | rien dans NKGui | les trois familles |
| système de greffons | rien | §20bis |
| mode retenu | rien, et **rien à porter** | l'exécution naturelle d'un document |

⚠️ **Le premier de cette liste est le chemin critique, et c'est le plus borné** :
la grammaire EBNF tient en une centaine de lignes, elle est écrite, et elle ne
dépend d'aucun des autres manques. **Les décisions de format sont ses entrées** —
les prendre maintenant ne coûte rien puisqu'il n'y a rien à refaire.

---

## 6. La règle que cette nuit impose

> **Avant d'écrire qu'une chose est à faire, chercher où elle pourrait déjà être
> faite — et écrire où l'on a cherché.**

Quatre fois cette nuit, j'ai décrit comme neuf ce qui existait. Les quatre fois,
une seule commande aurait suffi. Et la conclusion était juste à chaque fois : ce
n'est pas le raisonnement qui a manqué, **c'est la vérification avant de
publier**.

*Une spécification a l'autorité de son papier. Celui qui la lit ne sait pas si
l'auteur avait ouvert le code — sauf si l'auteur le dit.*

**Conséquence pratique** : chaque section des documents 3 et 5 devrait porter la
mention de ce qui a été vérifié dans le code, et de ce qui ne l'a pas été. Ce
document est le premier pas ; il n'est pas complet.
