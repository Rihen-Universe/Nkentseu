# Le document est la source — ce que `.nkgui` porte, et ce qu'il ne portera jamais

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**Décidé le 25/09/2026. Contrainte de conception permanente du format `.nkgui`.**

Ce document existe pour qu'une conclusion fausse ne soit pas retrouvée deux fois. Il
énonce **une règle**, **un avertissement** et **deux chemins d'export**. Il ne décrit pas
ce qui est fait aujourd'hui : il dit ce qui doit rester vrai.

---

## 1. LA RÈGLE

> **Le document NOMME des actions et EXPRIME des conditions. Il ne CALCULE jamais.**
> Tout ce qui demande un calcul est une **action nommée**, implémentée par l'hôte.

Le format s'y conforme déjà, et c'est un bon choix qu'il faut protéger :

```
Callback "alerte"     → une action NOMMÉE, avec ses arguments
bind                  → une liaison à une donnée vivante
if / Expression()     → un petit langage de conditions
```

Le document **nomme** ; l'hôte **implémente**, par un pointeur de fonction qu'il fournit
(`NkGuiCallbackFn`, cf. `NKGui/Doc/NkGuiInteraction.h`).

### Pourquoi cette règle, et ce qu'elle protège

Le format contient déjà `if`, des conditions, des branches et un évaluateur
d'expressions. **Tant que ce langage reste petit, tout tient.** Le jour où l'on y ajoute
des boucles, des variables, des fonctions ou de la manipulation de chaînes, **trois
choses cassent ensemble** :

1. **L'export cesse d'être une traduction et devient un compilateur.** Traduire
   `Callback "alerte"` vers un autre langage, c'est une ligne. Traduire un langage à part
   entière, c'est un projet — avec ses écarts et ses « ça marche dans l'éditeur, pas dans
   le produit ».
2. **La simulation et l'exécution divergent.** Deux évaluateurs, deux comportements, et
   le pire des défauts : celui qui n'apparaît que chez l'utilisateur.
3. **Le document cesse d'être DESSINABLE.** On ne trace pas une boucle `for` à la souris.
   Le jour où il faut taper du code dans NKUIDesign, on a réinventé un éditeur de texte
   et perdu la raison d'être du format.

**Usage pratique** : quand quelqu'un proposera « juste une petite boucle », cette règle
donne le droit de refuser, avec une raison plutôt qu'un goût.

---

## 2. ⚠️ L'AVERTISSEMENT — l'export part du DOCUMENT, jamais du runtime

**NKGui est en mode immédiat. HTML, Qt, et la plupart des cibles d'export sont en mode
retenu.** Cela ressemble à une incompatibilité de fond.

**Ce n'en est pas une, et voici pourquoi** : le format `.nkgui` est **déclaratif**. Il
décrit un **arbre de nœuds** avec des rôles, des ancrages et des propriétés — pas une
suite d'appels de fonctions. La conversion se fait donc depuis le **document**, jamais
depuis l'exécution.

> 🔴 **Quiconque tentera d'exporter en partant de NKGui au lieu du `.nkgui` conclura que
> c'est impossible — et il aura tort.** Cette note existe pour qu'il lise ceci avant de
> conclure.

Le mode immédiat est une **propriété du runtime**, pas du format. Le monteur
(`NkGuiMonteur.h`) traduit déjà le document déclaratif vers des appels immédiats : c'est
la preuve que le document est bien l'amont, et le runtime l'aval. Un exportateur est un
**autre aval** du même amont.

### ⚠️ Cet avertissement porte sur la SOURCE, jamais sur la CIBLE

Question posée par Rodolf le 25/09, et l'ambiguïté était dans la rédaction : *« mais on
pourra exporter vers NKGui pour les applications qui utilisent NKGui, n'est-ce pas ? »*

**Oui — et c'est la direction la plus facile de toutes.** Ce qui est impossible, c'est
d'exporter **en partant** de NKGui, c'est-à-dire en observant ses appels en mode
immédiat. **Viser** NKGui ne pose aucun problème : c'est un aval comme un autre.

```
              ┌──────────────┐
              │   .nkgui     │  ← LA SOURCE, toujours
              └──────┬───────┘
      ┌──────────────┼──────────────┬──────────────┐
      ▼              ▼              ▼              ▼
  NKGui (monté)  HTML+CSS      C++ généré      autres dorsaux
   au runtime     +WASM        (NKGui direct)   (greffons)
```

**Deux façons de viser NKGui** — et elles se cumulent, elles ne se choisissent pas.

⚠️ **Une première rédaction posait ici un faux dilemme** : elle présentait « monter au
runtime » et « générer du C++ » comme deux produits, chacun perdant ce que l'autre gagne.
Rodolf, le 25/09 : **« je ne veux rien perdre. »** Il a raison, et le dilemme n'existait
pas — ce sont **deux configurations de construction du même document**, comme Qt le fait
avec QML et Slint avec ses composants.

```
développement  →  l'application LIT le .nkgui sur le disque  →  on redessine sans recompiler
livraison      →  le document est EMBARQUÉ à la compilation  →  rien à livrer, rien à lire
```

### ⚠️ Le piège à éviter, et la conception qui l'évite

Si le générateur émet du C++ **qui appelle directement les widgets**, il existe alors
**deux implémentations de ce que signifie un document**. Elles divergeront — ce dépôt a
payé ce motif plusieurs fois (*deux compteurs sans code commun*, *une dérivation en
double*).

**La conception correcte : le générateur n'émet pas des appels, il émet l'arbre DÉJÀ
ANALYSÉ.**

| | qui interprète le document |
|---|---|
| développement | le monteur |
| livraison | **le monteur**, sur un arbre construit en mémoire au lieu d'être lu |

Une seule implémentation de la sémantique. La divergence devient impossible **par
construction**, pas par vigilance.

### Et le contrôle à la compilation s'obtient quand même

C'était l'argument sérieux en faveur du C++ généré. Il s'obtient sans lui, par deux
mécanismes bon marché :

1. **La validation à la construction.** `NkGuiValidate.h` existe déjà ; branché au build,
   un document fautif casse la compilation au lieu d'ouvrir une fenêtre vide.
2. **Un en-tête de signatures engendré.** Le document dit `Callback "sauvegarder"` → le
   générateur émet la déclaration que l'hôte doit satisfaire. L'hôte oublie de
   l'implémenter → **ça ne compile pas**.

### Ce que ça coûte réellement, et il faut le payer

**Un banc qui prouve que les deux configurations donnent le même résultat** : monter un
document depuis le disque, monter le même embarqué, comparer l'arbre obtenu ; s'il
diffère, rouge. C'est la seule chose qui empêche les deux chemins de se séparer en
silence, et elle n'est pas facultative.

📌 **Et une propriété qui vaut d'être notée** : NKGui devient **un dorsal parmi les
autres**, comme dans l'architecture NkSL. Mais celui-là est utilisé tous les jours — il
ne peut donc pas pourrir sans qu'on le voie. *Le dorsal de référence est celui dont on se
sert.*

---

## 3. LES DEUX CHEMINS D'EXPORT — et celui qu'il ne faut pas prendre

### ❌ Ce qui n'est PAS faisable : traduire du C++ vers un autre langage

Pas « difficile » : **pas faisable en général**. Le C++ a des templates, des pointeurs,
un préprocesseur, du comportement indéfini. Toutes les tentatives de C++ vers du
JavaScript lisible ont échoué ou se sont converties en WebAssembly. Et un greffon qui
lirait du C++ aurait besoin d'une façade de compilateur C++ — c'est clang, deux millions
de lignes. **Aucun utilisateur n'écrira ce greffon-là.**

### ✅ Chemin 1 — le web : on ne traduit pas, on **compile**

**Emscripten est une cible réelle et construite de ce dépôt** : `usetoolchain("emscripten")`
dans plusieurs `.jenga`, un dorsal NKWindow complet (canvas, événements, glisser-déposer,
point d'entrée).

```
.nkgui            →  HTML + CSS
le C++ de l'hôte  →  WASM
JavaScript        →  ne fait que les relier
```

Aucune traduction. **Le même C++ tourne dans NKUIDesign pour la simulation et dans le
navigateur pour le produit** — une seule source, un seul comportement, donc pas de « ça
marche dans l'éditeur mais pas dans le navigateur ».

### ✅ Chemin 2 — les autres langages : un transpileur à dorsaux, comme **NkSL**

Ce dépôt a déjà résolu ce problème une fois. `wiki/Runtime/NKRenderer/Materials-Shaders.md` :
*« convertit un source NkSL vers le langage du backend visé — c'est lui qui réalise la
promesse "écrire une fois" »*. **Un langage, un transpileur, cinq dorsaux.**

La même architecture appliquée à la logique d'interface donne ce qui est voulu : un
utilisateur qui veut Python, C# ou Rust écrit un **dorsal de transpileur**, pas un
compilateur. Un dorsal NkSL fait des centaines de lignes ; une façade C++ en ferait des
millions.

---

## 4. LA CONSÉQUENCE ARCHITECTURALE — ce qui rend les greffons écrivables

> **Un greffon consomme le DOCUMENT et les SIGNATURES d'actions. Jamais les corps en C++.**

Le document dit « ce bouton appelle `sauvegarder(nom)` ». Le greffon sait donc quoi
générer comme appel, dans n'importe quel langage, **sans rien connaître de
l'implémentation**.

C'est ce qui rend les greffons petits. Si un greffon doit comprendre le corps d'une
fonction, il n'y aura jamais qu'un seul greffon : le nôtre.

---

## 5. ⚠️ CE QUE CE DOCUMENT NE DÉCIDE PAS

**« Pour le futur » ne doit pas piloter aujourd'hui.** Concevoir pour HTML maintenant
tordrait le format pour un client qui n'existe pas. **La seule chose à faire aujourd'hui
est la règle du §1** — elle garde les deux portes ouvertes pour un coût nul, parce que
le format s'y conforme déjà.

Reste ouvert, et appartient à Rodolf :

- **Qui écrit le C++ des actions ?** Un développeur à côté du designer, ou le designer
  lui-même ? Si l'on attend d'un designer qu'il écrive du C++ pour voir son bouton
  réagir, on remet la barrière que le document venait de supprimer. Deux niveaux sont
  possibles — des **actions standard fournies** (ouvrir, fermer, basculer, naviguer,
  lier) que l'on compose sans écrire une ligne, et des **actions sur mesure** en C++ pour
  le reste. C'est une décision de produit, pas de technique.
- **Les variantes** : les applications ne partagent pas un document identique (Nogee a
  des spécificités liées à l'ECS). Base commune + variante par application — la façon
  dont le format l'exprime reste à définir.
- **Les animations et les blueprints** dans NKUIDesign : ambition nommée, pas de
  chantier ouvert.
