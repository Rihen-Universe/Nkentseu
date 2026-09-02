# Noge — ce qui attend une décision de toi

> Une page. Le détail est dans `Engine/Noge/echanges_noge.md` (rapport complet).
> Rien de ce qui suit n'est engagé : les quatre blocs attendent ta réponse.
> Branche `feat/noge-inventaire`, arbre `Nkentseu-noge`, 2026-09-02.

---

## 1. ⭐ L'IMAGE WEB — tu es la seule ressource qui manque, et ça te prend 2 minutes

La chaîne 3D web est **débloquée et vérifiée** : elle construit (30/30), elle lie,
elle initialise entièrement, elle exécute 22 passes par image, et il n'y a **plus
une seule erreur** dans le journal. Ce qui manque n'est pas du code : c'est **un
vrai GPU**. Je n'ai que SwiftShader (rendu logiciel), trop lent pour aboutir, et
je ne touche pas à ta carte pendant qu'Ilyana s'entraîne.

⚠️ **LIS ÇA D'ABORD — un serveur tourne DÉJÀ sur le port 9001** (PID 30152), et
il sert l'arbre **Debug** (vérifié : il rend un wasm de 35 052 605 octets, exactement
le fichier Debug). C'est peut-être le tien, lancé avant de te coucher : **ne le
tue pas**, ce n'est pas nécessaire. Mais `renderdemo.bat` prend **9001 par
défaut** — lancé sans argument, il **échouera à réserver le port sans le dire**,
et ton navigateur s'ouvrira quand même… **sur l'ancien serveur**. Tu croirais
tester le Release en testant le Debug. D'où le port explicite :

**Ce que tu lances :**

```
Build\Bin\Release-Web\renderdemo\renderdemo.bat 9002
```

puis dans ton navigateur :

```
http://localhost:9002/renderdemo.html?demo=2
```

**Vérifie en une ligne que c'est bien le Release qui répond** — la taille du wasm
est le témoin le plus simple :

```
curl -s -o NUL -w "%{size_download}\n" http://localhost:9002/renderdemo.wasm
```

| ce que tu lis | ce que ça veut dire |
|---|---|
| **27 444 289** | ✅ Release — c'est ce qu'on veut mesurer |
| 35 052 605 | ❌ Debug — tu es sur l'ancien serveur, change de port |

**Ce que tu dois regarder — pas « une image », ce HUD précisément :**

| ce que tu cherches | ce que ça prouve |
|---|---|
| la ligne **`Demo 3D \| API : ...`** en haut à gauche | ✅ la démo 3D tourne vraiment. **C'est LE test.** |
| le panneau **`== Shadow tweak (panel debug) ==`** à droite | ✅ le sous-système d'ombres est monté (`VSM atlas 4096 px`) |
| **`FPS approx :`** avec une valeur | ✅ la boucle rend des images |
| l'image | sphères PBR + **ombres portées** + grille de cubes |

⚠️ **`Draw:` et `Tris:` — deux corrections successives, voici l'état FINAL.**
J'avais d'abord écrit « non nuls = le test », puis « ne les regarde jamais » :
les deux étaient faux. La vérité est **datée** : les compteurs ont été branchés
le **5 août** (commit `7f3ada7b` — « les compteurs de rendu comptent enfin »,
dans la classe de base du tampon, donc tous backends). Tes captures du 29/07
affichaient `Draw:0` avec la 3D à 142 FPS parce qu'elles PRÉCÈDENT ce commit.
**Sur ton build actuel, ils font foi : attends-toi à du non-nul** (le Web
logiciel affiche `Draw:1093 Tris:489586`). Mais le test PRINCIPAL reste la
ligne `Demo 3D` + le panneau d'ombres — eux valent sur n'importe quel binaire.

⚠️ **Le vrai signe d'échec, celui d'HarmonyOS** : un fond **uni**, avec
seulement `Active: R2D|R3D|TEXT|OVERLAY` et **aucune ligne `Demo 3D`**, **aucun
panneau Shadow tweak**. C'est ça, « le moteur vit, la 3D non ».

Si la ligne `Demo 3D` et le panneau d'ombres sortent avec de la géométrie, **la
cible Web passe au vert**.

🔵 **Si le résultat te surprend, le contrôle le moins cher est déjà sous ta
main** : le Debug est servi sur `http://localhost:9001/renderdemo.html?demo=2`,
il est à jour (reconstruit avec les mêmes correctifs) et seulement **plus lent**.
Deux builds différents qui donnent la même image confirment le résultat ; deux
images différentes désignent la configuration, pas le moteur.

---

## 2. Les trois doublures CPU — **quand**, pas **si**

`NkClothSystem`, `NkHairSystem`, `NkSoftBodySystem` sont en **compute GPU**, et
**WebGL2 n'a pas de compute** (mesuré). Sur une de tes sept cibles, ils ne
pourront jamais tourner tels quels.

**Le principe est déjà tranché par ta propre règle** — *ce qui ne peut pas se
faire doit avoir une doublure crédible, jamais un trou*. Donc la question n'est
pas s'il faut une doublure, mais **quand** :

- **maintenant** — le module est cohérent sur les 7 cibles, mais ça retarde la course ;
- **après la course** — on avance sur le jeu, et ces 3 systèmes restent absents du Web.

🔵 **Ma recommandation : après la course.** La course de voitures n'utilise aucun
des trois (tissu, cheveux, corps mous sont du personnage). **Bloqué tant que tu
n'as pas répondu :** rien — je ne les rouvre pas.

---

## 3. 🔴 `NkSkeleton` pèse **77 064 octets** — dimensionnement à trancher

Mesuré : `sizeof(NkSkeleton)` = **75 Ko**. `bones[256]` + `skinMatrices[256]` en
tableaux fixes, pour des squelettes qui en utilisent quelques-uns.

Dans un ECS les composants sont rangés **par valeur**. Donc :
- **100 personnages = 7,5 Mo** de squelettes ;
- **75 Ko recopiés** à chaque changement d'archétype ;
- quatre exemplaires sur la pile **font planter** un programme (c'est comme ça
  que je l'ai trouvé : mon banc est tombé en dépassement de pile).

🔵 **Ma recommandation : passer les tableaux en allocation dynamique**, ou baisser
`kMaxBones` à une valeur réaliste (64 ?). **Je n'y touche pas** : c'est ton code,
d'autres modules le consomment, et c'est une décision d'architecture.

---

## 4. `.gitattributes` et les shaders `.nksl` — piège rétroactif

Constat : les `.nksl` sont en **CRLF sur le disque**, **LF dans l'index**, et
`.gitattributes` n'a **aucune règle** pour eux — alors que ce sont des données
lues **octet pour octet** par le compilateur de shaders.

⚠️ **Pourquoi je ne l'ai pas corrigé** : ajouter une règle de fin de ligne
**renormalise tout le jeu de fichiers** au prochain checkout. Un dépôt entier de
shaders réécrit en silence, ce n'est pas quelque chose à déclencher sans toi.

🔵 **Ma recommandation : ajouter `*.nksl text eol=lf`, mais à un moment choisi**,
avec un arbre propre et une vérification après. Pas maintenant.

---

---

## 5. 🔎 Ton témoignage, et ce que la relecture change

**Ce que tu as dit** (2026-09-02) : *« sur HarmonyOS et Linux j'ai vu, même sur
Web »*, puis *« j'ai donc testé la démo `--demo=` ce jour-là, ce qui a fonctionné
sur toutes les plateformes sauf iOS et macOS »*, et sur le chemin : **« NKRHI,
NKRenderer »** — donc le bon, celui qui porte Noge. Tu posais toi-même la
réserve « il faut encore vérifier ». Je l'ai traité comme un indice.

### ✅ LINUX : trace trouvée, et c'est le bon chemin

**`Captures/plateforme_linux.png`**, **29/07 à 15h38** — citée nulle part, elle
dormait dans le dépôt. Scène 3D complète : `Demo 3D | API : OpenGL`, panneau
`Shadow tweak` (`VSM atlas 4096 px`), sphères PBR, ombres portées, **81,6 FPS**,
et `[Phase H] Texture file-based : test_pattern.png LOAD OK`.

⭐ **Jumelle** : `plateforme_windows.png`, **15h34** — même scène, même cadrage,
**142,1 FPS**. Une comparaison Windows/Linux délibérée, en une session.

⚠️ Rien *dans l'image* ne nomme le système : l'attribution repose sur le nom du
fichier et l'appariement. **Un mot de toi la ferme.**

### 🔴 HARMONYOS : je m'étais trompé, et voici la date exacte de l'explication

J'avais classé HarmonyOS en échec. **Relecture faite, le verdict était mal
cadré** — et ce que je citais (`Draw:0 Tris:0`) est le témoin qu'on sait
maintenant sans valeur.

Relu avec le bon témoin : **aucune ligne `Demo 3D`**, **aucun panneau
`Shadow tweak`**, **aucun `FPS approx`**. Et `PLATEFORMES_ETAT.md:182` le dit
lui-même : *« IDENTIQUE au HUD renderdemo (**demo 0 Subsystems**) »*.

> **Cette capture montre la démo 0. Elle n'a jamais exercé la 3D.** Ce n'est pas
> une preuve que la 3D échoue sur HarmonyOS — c'est une preuve qu'elle n'a pas
> été testée.

**Et la chronologie explique tout l'écart avec ton souvenir :**

| date | événement |
|---|---|
| **29/07 19h09** | la capture HarmonyOS est prise → **démo 0** |
| **09/08 23h33** | commit `a762bda8` — *« demo 3D par defaut »*, pose `NK_DEFAULT_DEMO=2` |

**La capture précède de onze jours le correctif qui fait démarrer HarmonyOS en
3D.** Ton souvenir et ma mesure ne se contredisent pas : ils parlent de deux
binaires différents. **HarmonyOS repasse de ❌ à ❔.**

### 🔴 WEB : même relecture, même conclusion

`PLATEFORMES_ETAT.md:158-162` dit `Execute frame=1 : passes=1` et *« screenshot
pris **trop tôt**, virtual-time-budget »*. Ma mesure d'aujourd'hui donne **22
passes par image**. La capture a été prise **à la première image, une seule
passe** : elle testait la création du contexte, pas le rendu 3D. **Web aussi
repasse de ❌ à ❔.**

### 🛠️ HarmonyOS : le chemin praticable, pour quand le GPU sera libre

Il n'y a **aucun `.hap` construit** aujourd'hui — il faut le refaire. Bonne
nouvelle : **tu n'as plus rien à sélectionner**, la démo 3D est figée à la
compilation depuis le 09/08.

```
jenga build --target renderdemo --platform HarmonyOS --config Release
```

puis installe le `.hap` produit sous `Build/Bin/Release-HarmonyOS/renderdemo/`
sur l'émulateur, et lance.

**Ce que tu dois voir** — et surtout **plus jamais `Draw:`/`Tris:`** :

| ce que tu cherches | verdict |
|---|---|
| ligne **`Demo 3D \| API : ...`** + panneau **`Shadow tweak`** | ✅ la 3D tourne |
| aplat uni + seulement `Active: R2D\|R3D\|TEXT\|OVERLAY`, sans ligne `Demo 3D` | ❌ c'est la démo 0, comme en juillet |

---

---

## 6. 🦴 Le squelette : fait « comme Unreal » — trois choix restent à toi

`sizeof(NkSkeleton)` : **77 064 → 88 octets**. Actif partagé + par-instance au
réel, tous les consommateurs migrés et re-exécutés verts, banc contre-éprouvé.
Détail : rapport, mesure 9.

Pour qu'il serve « à tout système qui gère les animations squelettiques »
(tes mots), il reste trois choix — je n'en ai tranché aucun :

1. **L'étage de `NkSkeletonDef`** (l'actif). Candidats : `NKAnima` (le
   substrat extrait de NKRenderer le 14/08, où vit déjà le reciblage) ou
   `NKAnimPhysics`. ⚠️ Noge ne dépend d'aucun des deux aujourd'hui : descendre
   l'actif AJOUTE une dépendance.
2. **La convention de pose de repos.** Ton `NkRetargetSkeleton` (noyau) stocke
   du LOCAL relatif au parent ; le squelette Noge stocke des matrices
   bind/inverse-bind. L'un des deux devra se convertir vers l'autre.
3. **Un nom.** Il existe maintenant deux `NkBoneDef` — le mien (os de rendu) et
   celui du ragdoll (corps physique). Espaces de noms distincts, ça compile,
   mais c'est le motif `NkShaderStage` : lequel renomme-t-on ?

---

## 7. 🔌 Les greffons : conception posée et chiffrée — à toi de lancer

Ton audit de consommation est fait (rapport, mesure 10) : 24 modules consommés,
deux vérités rétablies dans `_DEPS` (`NKAnima`, `NKCanvas` — exercés sans
être déclarés, corrigé, 41/41), un seul déclaré-inerte réel (`NKAnimPhysics`,
proposition : l'exercer quand un jeu jouera l'équilibre — PV3DE, pas la course).

**`NKXR` et `NKCamera`, que tu as nommés, ne sont PAS câblés en dur — exprès.**
Ta piste greffons est la bonne réponse : un jeu de course n'embarque pas
OpenXR, et chaque « dû » câblé en dur grossit tous les binaires (le wasm fait
déjà 27 Mo). La conception est écrite (§10.4 du rapport) : `NkSharedLib`
d'abord (15 sites recopient `LoadLibrary`/`dlopen` à la main aujourd'hui —
mesuré), frontière **C pur versionnée** (zéro-STL et ABI C++ ne traversent pas
une DLL), doublure crédible obligatoire, et **un seul système pour la maison**
(moteur + panneau Greffons de NkUIDesign + `Extensions/` de NkCode).

**Chiffrage : ~1,5 à 2 semaines** pour `NkSharedLib` + la frontière + les deux
premiers greffons (`xr` avec le simulateur sans casque, `camera`), chacun aux
trois conditions (corps, appelant, banc qui rougit débranché).

**Ta décision** : lancer ce chantier — maintenant, ou après la course ?

---

## 8. 🦴 NkAnima : la bibliothèque est renommée — le DOSSIER attend un mot de toi

**Fait (phase 2)** : `NKAnimation` → **`NKAnima`** (casse du noyau, `NK`).
48 fichiers, 118 occurrences, `git mv` pour que l'historique suive. Deux bonnes
surprises : l'espace de noms est `nkentseu::anim` (**aucun namespace touché**) et
le nom `NKAnima` était **libre**. Preuves : Noge 41/41, LocomotionDemo **9/0**,
AssetIODemo **55/0**, SystemsRevivalTest **34/0**, NkAnimPhysTest 27/27 — les
comptes sont **identiques à l'avant-renommage**, aucun banc n'a disparu.

🔴 **Ce que je n'ai PAS fait, et pourquoi.** Tu as dit « on ne doit pas avoir
NkAnima ET NkAnimaEditor ». Mesure faite avant de toucher :

| | `Applications/NkAnima` | `Applications/NkAnimaEditor` |
|---|---|---|
| code | **0 ligne** | 1 859 lignes |
| `.jenga` | **aucun** | ✅ |
| contenu | **`ROADMAP.md`, 598 lignes** | l'application |

**`Applications/NkAnima` n'est pas une application : c'est un dossier qui porte
un document** — et `Nkentseu/CLAUDE.md` y renvoie **deux fois** (lignes 65 et 68,
« à lire au démarrage »). Le supprimer casserait ces deux liens et détruirait
598 lignes de pilotage. Il n'y a donc pas deux applications à fusionner : il y a
**une application et un document mal rangé**.

**Ta décision — trois options, aucune ne supprime :**
1. le document rejoint l'application → `NkAnimaEditor/ROADMAP.md` ;
2. il rejoint la bibliothèque → `Kernel/Runtime/NKAnima/ROADMAP.md` (il parle
   surtout de jalons moteur : IK, blend, physique de pose) — **ma préférence** ;
3. il reste, et `Applications/NkAnima/` est assumé comme dossier de doc.

Dans les cas 1 et 2, les deux liens de `CLAUDE.md` sont à corriger — je le ferai
dans le même geste.

---

## Ce qui est fait et ne t'attend pas

- **Web débloqué** : garde EGL (`NK_OPENGL_ES` ne veut pas dire « EGL disponible »)
  → 23/30 ✗ **→ 30/30 ✓** ; shader `PP_AutoExposure` (bloc divergent entre étages)
  → `valid=0` **→ `valid=1`**, zéro erreur.
- **Textures FBX** : elles étaient cherchées **un dossier trop haut** → la voiture
  charge maintenant ses 3 cartes (0/3 → **3/3**).
- **`quality` est opérant** : l'enum passait de « écrit 7 fois, lu 0 fois » à
  l'axe qui pilote le rendu, + `ForTarget()` qui choisit le profil **dans le
  moteur** (l'appli ne nomme plus ni plateforme ni preset).
- **`Noge/Systems` : 3 systèmes sur 6 réanimés** (jiggle, mocap, ragdoll), chacun
  avec un corps, un appelant réel et un banc contre-éprouvé.
- **Journal web** : 12 411 lignes de diagnostic par exécution → **0** (extinguible).
- **`QueryCaps`** ne fabrique plus de capacités fausses et plausibles.
