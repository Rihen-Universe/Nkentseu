# Ce qu'on remet aux étudiants

Document de référence pour l'enseignant. Il dit **quoi donner**, **dans quel
ordre**, et **ce qu'on attend en retour**.

---

## 1. Le paquet

Un seul fichier : **`ConquerorLab-Kit.zip`** — 5,7 Mo.

Produit par `Applications/ConquerorLab/Distribuer.ps1 -Zip`, il se décompresse
n'importe où et fonctionne sans installation.

```
ConquerorLab.exe            l'atelier
libgcc_s_seh-1.dll         les trois bibliothèques d'exécution
libstdc++-6.dll            (ne pas les déplacer : l'exe ne démarre pas sans)
libwinpthread-1.dll
Cours_ConquerorLab.pdf     le cours, 8 chapitres
VERSION.txt                date de construction + versions d'ABI
LISEZMOI.txt               installation du compilateur, premiers gestes

include/                   TOUS les en-têtes, à plat — un seul -I
    Conqueror/             le contrat (3 fichiers)
    NKCore/ NKMath/ NKContainers/ NKLogger/ NKFileSystem/
    NKThreading/ NKTime/ NKStream/ NKMemory/ NKPlatform/
    NKSerialization/ NKReflection/

lib/                       12 bibliothèques statiques, liées automatiquement

travail/                   LEUR ESPACE DE TRAVAIL
    rules/                 leurs moteurs de règles      (.cpp)
    ai/                    leurs IA                      (.cpp)
    boards/                leurs grilles                 (.json)
    totems/                leurs images                  (.png)

exemples/
    rules/RegleMinimale.cpp    le plus petit moteur qui joue vraiment
    rules/GrilleLibre.cpp      un plateau circulaire défini en C++
    ai/IAMinimale.cpp          la plus petite IA qui joue vraiment
    boards/                    18 plateaux, recopiés dans travail/ au 1er lancement
    totems/LISEZMOI.txt        les règles de nommage des images
```

> **Windows x64 uniquement.** Ce n'est pas une limite de conception — les
> portages Linux, Android et Web de NKGui existent — mais une limite de ce qui a
> été **construit et vérifié**. Un étudiant sous Linux ou macOS clone le dépôt et
> compile (`jenga build --target ConquerorLab --config Release`) ; qu'il signale
> ce qui casse, c'est utile.

### Le seul prérequis

Un compilateur C++ sur la machine, parce que **l'atelier compile leur code
pendant qu'il tourne**. La marche à suivre est dans `LISEZMOI.txt` :

```
1. installer MSYS2                     https://www.msys2.org
2. ouvrir « MSYS2 UCRT64 » et taper :
       pacman -S mingw-w64-ucrt-x86_64-clang
3. relancer ConquerorLab.exe
```

Le panneau *Modules* affiche en clair le compilateur trouvé — ou dit qu'il n'en a
trouvé aucun. **Faites cette installation en séance**, avant tout le reste : un
étudiant bloqué là n'apprendra rien du cours.

---

## 2. Ce qu'on remet en plus du paquet

| Document | Rôle | Quand |
|---|---|---|
| **`SUJETS.md`** | les 60 sujets, avec description et objectif | séance 1 |
| **Ce document** | ce que vous lisez | vous seul |
| `Applications/ConquerorLab/CARNET.private.md` | le journal de conception réel du projet, erreurs comprises | séance 2, **en modèle de livrable** |

Le carnet mérite un mot. C'est le journal daté de la conception de cet outil,
**avec ses erreurs** : le réglage d'ouverture qui faisait croire à une panne, le
`LISEZMOI.txt` livré avec 22 caractères corrompus, l'option « 4 joueurs » qui ne
pouvait produire qu'une partie perdue d'avance, le bug de disposition d'ABI qui a
provoqué un plantage. Donnez-le comme **modèle de ce qu'on attend d'eux**, et
comme premier exercice de critique.

---

## 3. L'ordre de distribution

**Séance 1 — installer, jouer, lire.**
Le paquet. Installation du compilateur, lancement, une partie humain contre IA.
Rien d'autre. L'objectif est qu'ils aient l'outil qui tourne et qu'ils l'aient
*utilisé* avant d'en parler.

**Séance 2 — critiquer.**
`SUJETS.md` et le carnet. Chacun choisit trois sujets et dit pourquoi. Discussion
sur ce que le carnet montre d'une conception réelle.

**Séance 3 et suivantes — concevoir.**
Un sujet par étudiant ou par binôme.

---

## 4. Ce qu'on attend en retour

Trois pièces, et **la première pèse le plus**.

### 4.1 Le carnet de conception (50 %)

Un fichier Markdown daté, sur le modèle de `CARNET.private.md`. Il doit contenir,
pour chaque décision :

- **ce qui a été observé** — pas ce qui a été imaginé ;
- **les options envisagées**, y compris celles écartées ;
- **pourquoi celle-ci** ;
- **ce qui s'est révélé faux** en cours de route.

> Une entrée qui ne contient aucune option rejetée et aucune erreur n'est pas un
> carnet de conception : c'est un compte rendu écrit après coup.

### 4.2 L'observation d'un utilisateur (30 %)

Dix minutes de captation vidéo d'une **autre personne** utilisant leur outil,
**sans être aidée**. À rendre :

- l'enregistrement ;
- les trois moments où l'utilisateur a hésité, avec l'horodatage ;
- ce qu'ils en ont changé.

C'est la pièce qu'aucun outil ne peut produire à leur place : personne ne peut
regarder quelqu'un d'autre se débattre à leur place.

### 4.3 La réalisation (20 %)

L'outil, la maquette ou la spécification, selon le sujet. Elle compte le moins,
et ce déséquilibre est **volontaire et annoncé** : on évalue la conception, pas
la production.

### 4.4 La soutenance

Dix minutes, trois questions :

1. **Pourquoi ce choix et pas l'autre ?**
2. **Qu'est-ce qui casse si l'utilisateur fait X ?** (X choisi par vous, à
   l'instant)
3. **Qu'est-ce que vous n'avez pas fait, et pourquoi ?**

Ces trois questions suffisent à distinguer qui a conçu de qui a assemblé.

---

## 5. Sur l'usage de l'IA

À dire aux étudiants **au début, franchement** : cet atelier a été écrit avec
l'aide d'une IA, et le carnet le montre. Interdire serait à la fois inapplicable
et malhonnête.

Ce qui est demandé est autre chose : **comprendre avant de déléguer**. Le
dispositif d'évaluation ci-dessus y suffit sans interdiction, parce qu'une IA ne
produit ni observation d'un utilisateur réel, ni chaîne de décisions rattachée à
ce qui a été vu, ni réponse tenable à « qu'est-ce qui casse si… ».

Un levier supplémentaire, structurel : ce dépôt est **idiosyncratique** — moteur
propriétaire, ABI maison, commentaires en français, conventions qui ne sont nulle
part ailleurs. Les modèles y sont nettement moins utiles que sur du Unity ou du
React, non par obstacle artificiel, mais parce qu'il n'y a rien à recracher.

---

## 6. Régénérer le paquet

```powershell
cd Applications\ConquerorLab
.\Distribuer.ps1 -Zip
```

Le PDF du cours doit être à jour avant, sinon la version embarquée sera l'ancienne :

```powershell
cd Documentation\cours-conqueror
.\build.ps1
```

> **Fermez le PDF avant de relancer `Distribuer.ps1`.** Un lecteur ouvert
> verrouille le fichier et le script échoue au nettoyage.

`VERSION.txt` porte la date de construction. **Demandez qu'elle soit citée dans
tout retour** : sans elle, on corrige un défaut déjà corrigé.
