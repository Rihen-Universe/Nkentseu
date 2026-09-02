# Noge — ce qui attend une décision de toi

> Une page. Le détail est dans `Engine/Noge/echanges_noge.md` (rapport complet).
> Branche `feat/noge-inventaire`, arbre `Nkentseu-noge`.
> **Mise en ordre du 2026-09-02, en fin de journée.**

## 📌 CE QUI RESTE, ET CE QUI NE T'ATTEND PLUS

**Cinq des huit blocs sont tranchés.** Ils restent écrits, avec leur réponse et
sa date — *une feuille qui garde des questions déjà répondues fait perdre du
temps ; une feuille qui efface l'historique fait re-trancher.*

### 🟠 CE QUI T'ATTEND ENCORE — quatre choses, et trois sont des minutes

| # | ce qu'il faut | de qui | coût |
|---|---|---|---|
| **A** | **Confirmer le Web sur un VRAI GPU.** La cible est verte en rendu **logiciel** (SwiftShader) ; il manque une exécution sur ta carte. Mode d'emploi complet au **bloc 1**. | **toi** | ~2 min de navigateur |
| **B** | **Le `.hap` HarmonyOS à reconstruire**, puis l'installer sur l'émulateur. HarmonyOS n'a **jamais** exercé la 3D : sa seule capture (29/07) montre la démo 0, et le correctif « démo 3D par défaut » date du 09/08. Détail et commandes au **bloc 5** et à la **carte**. | moi, dès que le GPU/l'émulateur sont libres | ~30 min |
| **C** | **Re-tester Linux sous WSL.** Le vert repose sur la capture du 29/07 + ton témoignage ; le build d'aujourd'hui n'y a jamais tourné. WSL2 n'a pas répondu en 120 s pendant cette session. | toi (débloquer WSL), puis moi | ~10 min |
| **D** | 🦴 **Où vit `NkSkeletonDef`** — la seule vraie décision d'architecture qui reste. Détail et candidat mesuré au **bloc 6**. | **toi** | une phrase |

### ✅ CE QUI EST TRANCHÉ — n'y reviens que si tu changes d'avis

| bloc | question | réponse | date |
|---|---|---|---|
| 2 | doublures CPU cloth / hair / softbody | **après la course** (la course n'en utilise aucun) | 02/09 |
| 3 | `NkSkeleton` pesait 77 064 octets | **fait — 88 octets**, actif partagé façon `USkeleton`, tous les consommateurs verts, banc contre-éprouvé | 02/09 |
| 4 | `.gitattributes` et les shaders `.nksl` | **fait** — `*.nksl text eol=lf`, normalisation délibérée avec preuve après (commit `58d7e07d`, 14h35) | 02/09 |
| 7 | les greffons (`NkSharedLib`, frontière C, `xr`/`camera`) | **APRÈS la course** — conception et chiffrage restent en réserve, rien n'est rouvert | 02/09 |
| 8 | le dossier `Applications/NkAnima` | **option 2** — le document a rejoint la bibliothèque, `Kernel/Runtime/NKAnima/ROADMAP_PRODUIT.md` ; dossier vide retiré, aucune ligne perdue | 02/09 |

📍 **La carte des 7 plateformes est en bas de cette page, section 9.**

---

## 1. ⭐ L'IMAGE WEB — 🟡 VERT AVEC RÉSERVE ; il te reste 2 minutes à donner

> ### 🟡 RÉPONDU LE 2026-09-02 — la cible est VERTE, la réserve est nommée
> **Ce qui a été obtenu depuis que ce bloc a été écrit** :
> `Captures/plateforme_web_2026-09-02.png` — HUD lu ligne à ligne :
> `Demo 3D | API : …`, panneau `Shadow tweak` (`VSM atlas 4096 px`),
> `Draw:1093 Tris:489586`. **La 3D rend dans un navigateur.** Web est la
> **4ᵉ cible verte**.
>
> ⚠️ **La réserve, et elle est réelle** : ce rendu est **LOGICIEL**
> (SwiftShader, ~9 h pour aboutir), sur le binaire **Debug** post-EGL. Ça prouve
> que la chaîne complète — contexte, shaders, 22 passes, ombres — est correcte.
> Ça ne prouve **rien sur les performances**, et ça n'a jamais touché un pilote
> GPU réel. *Un rendu logiciel valide la logique, pas le matériel.*
>
> 👉 **Ce qui t'attend (point A du sommaire)** : les 2 minutes ci-dessous, sur ta
> carte. Le mode d'emploi qui suit reste **entièrement valable**.

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

## 2. ✅ TRANCHÉ (02/09) — les trois doublures CPU : **APRÈS LA COURSE**

> **Réponse : après la course**, comme recommandé plus bas. La course de voitures
> n'utilise ni tissu, ni cheveux, ni corps mous — ce sont des systèmes de
> personnage. Conséquence assumée et écrite : **ces 3 systèmes restent absents du
> Web** jusque-là (WebGL2 n'a pas de compute), et le module n'est donc pas
> cohérent sur les 7 cibles pendant ce temps. **Rien n'est rouvert.**
>
> *Le texte d'origine est conservé ci-dessous : c'est lui qui porte le
> raisonnement, et il redeviendra la feuille de route le jour où on lancera.*

### La question, telle qu'elle était posée — **quand**, pas **si**

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

## 3. ✅ FAIT (02/09) — `NkSkeleton` : **77 064 → 88 octets**

> **Ce bloc est résolu, et pas par une décision : par une mesure.** Le
> dimensionnement retenu est celui d'Unreal — un **actif partagé** (`USkeleton`)
> plus un par-instance dimensionné au réel. `sizeof(NkSkeleton)` passe de
> **77 064 à 88 octets**. Tous les consommateurs ont été migrés et ré-exécutés
> **verts**, le banc a été contre-éprouvé, et la pile ne déborde plus.
>
> 📌 **Le recensement s'est fait au COMPILATEUR, pas au `grep`** — et c'était
> nécessaire : `NkAssetIODemo` atteignait le type par un **champ**, sans jamais
> écrire son nom. Le `grep` l'avait raté.
>
> ⚠️ **Ce qui reste ouvert n'est plus la taille, c'est le PLACEMENT** de
> `NkSkeletonDef` → **bloc 6, point 1** (point D du sommaire).
>
> *Chiffres d'origine conservés ci-dessous — ils disent pourquoi c'était urgent.*

### Le constat d'origine — 🔴 `NkSkeleton` pesait **77 064 octets**

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

## 4. ✅ FAIT (02/09, 14h35) — `.gitattributes` : les `.nksl` sont en LF

> **Réponse : la règle a été ajoutée**, exactement comme recommandé — à un moment
> choisi, arbre propre, vérification après. Commit `58d7e07d`.
>
> ```
> *.nksl text eol=lf
> ```
>
> **Décision de toi (02/09, déléguée au coordinateur)** : normalisation
> **délibérée, avec preuve après** — plutôt que de la subir un jour par accident.
> Le motif est écrit dans le fichier lui-même : les 131 `.nksl` étaient
> `i/lf w/crlf` sous `text=auto`, et *c'est la seule famille de défauts qui
> traverse une revue entière par construction — aucun diff ne la montre, la copie
> de l'auteur est déjà juste.*
>
> *Le raisonnement d'origine est gardé ci-dessous : il explique pourquoi on ne
> déclenche pas ça un jour au hasard.*

### Le constat d'origine — piège rétroactif

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

## 5. 🔎 Ton témoignage, et ce que la relecture change — ✅ LINUX VERT, ❔ HARMONYOS OUVERT

> **Ce que ce bloc a produit, au 02/09** :
> - ✅ **Linux passe au VERT** — capture `Captures/plateforme_linux.png` (29/07,
>   15h38) **+ ton témoignage du 02/09**. La réserve d'attribution ci-dessous est
>   levée par ton mot. ⚠️ Reste le **re-test sous WSL** (point C du sommaire) :
>   le vert porte sur le 29/07, pas sur le build d'aujourd'hui.
> - ❔ **HarmonyOS reste OUVERT** — c'est le point B du sommaire, et c'est du
>   travail, pas une décision : reconstruire le `.hap`. Marche à suivre à la fin
>   de ce bloc, et carte en section 9.
> - ✅ **Web** : traité au bloc 1 (vert avec réserve).

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

## 6. 🦴 Le squelette : fait « comme Unreal » — 🟠 LE POINT 1 EST LA DÉCISION QUI RESTE

> 🟠 **C'est le point D du sommaire, et le seul arbitrage d'architecture encore
> ouvert de toute cette feuille.** Les points 2 et 3 en dépendent : la convention
> de pose de repos et le doublon de nom ne se tranchent qu'une fois l'étage
> choisi. **Candidat mesuré : `NKAnima`** — c'est là que vivent déjà le reciblage
> (`NkAnimRetarget`, 660 l.) et le mélange (`NkBlendTree1D/2D`), et le module est
> **pur Foundation, sans GPU**, donc il ne tire rien derrière lui.
> ⚠️ **Le prix, dit d'avance** : Noge ne dépend aujourd'hui **ni** de `NKAnima`
> **ni** de `NKAnimPhysics` — descendre l'actif **ajoute une dépendance**.
> 📌 *La bibliothèque s'appelle `NKAnima` depuis le 02/09 (ex-`NKAnimation`),
> et sa feuille produit est `Kernel/Runtime/NKAnima/ROADMAP_PRODUIT.md`.*

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

## 7. ✅ TRANCHÉ (02/09) — les greffons : **APRÈS LA COURSE**

> **Ta décision : après la course.** Le chantier n'est **pas ouvert** :
> `NkSharedLib`, la frontière C versionnée, les greffons `xr` et `camera` restent
> **conçus et chiffrés, en réserve** — rien n'est commencé, rien n'est à défaire.
> Les 15 sites qui recopient `LoadLibrary`/`dlopen` à la main restent tels quels
> jusque-là ; c'est un coût connu, pas un oubli.
>
> ⚠️ **Ce que cette décision NE remet PAS en cause** : les deux vérités rétablies
> dans `_DEPS` le même jour (`NKAnima`, `NKCanvas` — exercés sans être déclarés,
> corrigé, **41/41**) sont **acquises**. Elles ne dépendaient pas des greffons.
>
> *Chiffrage et conception conservés ci-dessous — c'est le dossier prêt à
> rouvrir, tel quel, le jour où tu diras oui.*

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

## 8. ✅ TRANCHÉ ET FAIT (02/09) — NkAnima : le dossier est rangé, rien n'a été perdu

**Fait (phase 2)** : `NKAnimation` → **`NKAnima`** (casse du noyau, `NK`).
48 fichiers, 118 occurrences, `git mv` pour que l'historique suive. Deux bonnes
surprises : l'espace de noms est `nkentseu::anim` (**aucun namespace touché**) et
le nom `NKAnima` était **libre**. Preuves : Noge 41/41, LocomotionDemo **9/0**,
AssetIODemo **55/0**, SystemsRevivalTest **34/0**, NkAnimPhysTest 27/27,
**Nogee 45/45** — les
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

### ✅ RÉPONSE APPLIQUÉE LE 2026-09-02 — option 2

```
Applications/NkAnima/ROADMAP.md  ->  Kernel/Runtime/NKAnima/ROADMAP_PRODUIT.md
```

**`ROADMAP_PRODUIT.md`, et non `ROADMAP.md` : la place était prise.** Le module a
déjà son `ROADMAP.md` (écrit le 17/08) qui décrit **le module** et renvoie
explicitement au **parcours produit**. Écraser l'un par l'autre aurait détruit un
document pour en ranger un autre. Les deux cohabitent, chacun dit en tête ce
qu'il est.

- `git mv` — l'historique suit, **aucune ligne supprimée** (613 l. intactes) ;
- **les deux liens de `Nkentseu/CLAUDE.md` (l. 65 et 68) sont corrigés** —
  ⚠️ ce fichier est **gitignoré**, donc hors commit et non versionné : il a été
  corrigé à la main, avec une note datée disant que le nouveau chemin n'existe
  que sur `feat/noge-inventaire` tant que la branche n'est pas fusionnée
  (l'arbre principal porte encore `NKAnimation` et `Applications/NkAnima`) ;
- **8 autres renvois suivis** (`Kernel/Runtime/NKAnima/ROADMAP.md` ×6,
  `Kernel/AI`, `NKGraph`, `NKPhysics`, `NkAnimPhysTest/src/main.cpp:311`,
  `NKIlyana/ROADMAP.md:884`, `Engine/Noge/ROADMAP.md:1758`) ;
- **`Applications/NkAnima/` retiré APRÈS vérification** qu'aucun `.jenga` ne le
  nomme et qu'il n'est pas dans la liste `include(...)` de `Nkentseu.jenga` — le
  registre des projets est **explicite**, pas un balayage de dossiers ;
- **les blocs datés qui constatent l'ancien chemin n'ont pas été réécrits**
  (celui du 23/07 dans le document déplacé, le §12.1 du rapport, cinq carnets
  hors dépôt). *On ne corrige pas un journal, on le date.*

📌 **Et « on ne doit pas avoir NkAnima ET NkAnimaEditor » est satisfait sans
supprimer quoi que ce soit** : il n'y avait pas deux applications, il y avait une
application (`Applications/NkAnimaEditor`) et un document mal rangé.

---

---

---

## 9. 🗺️ LA CARTE DES 7 PLATEFORMES — **4 vertes**, et ce qu'il faut pour les 3 autres

> **Ce qui est mesuré ici, c'est le chemin 3D** (`NKRHI` + `NKRenderer`) — le seul
> que Noge emprunte. ⚠️ **Le socle 2D (`NKCanvas`) est porté sur les sept** : la
> fenêtre, le contexte GL, les entrées et le 2D fonctionnent partout. Confondre
> les deux, c'est ce qui a fait croire pendant des semaines que « tout marchait »
> **et** que « rien ne marchait » — les deux affirmations étaient vraies, et
> parlaient d'objets différents.
>
> 🔴 **LE TÉMOIN, ET IL EST UNIQUE.** Une cible est verte quand le HUD affiche la
> ligne **`Demo 3D | API : …`** ET le panneau **`== Shadow tweak ==`**.
> **`Draw:` / `Tris:` ne servent qu'après le 5 août** (commit `7f3ada7b`, où les
> compteurs ont été branchés) : toute capture antérieure affiche `Draw:0` même
> quand la 3D tourne à 142 FPS. *Un témoin invalidé contamine tout ce qu'il a
> jugé* — c'est lui qui a fait classer HarmonyOS en échec à tort.

| # | cible | 3D | preuve | ce qui manque |
|---|---|---|---|---|
| 1 | **Windows** | ✅ | backend de référence, `plateforme_windows.png` (29/07, 142,1 FPS) | — |
| 2 | **Android** | ✅ | `Captures/nk_android_demo3d.png` — 18 sphères PBR, ombres, **59 FPS**, `VSM atlas 4096 px` | — (⚠️ textures **procédurales**, pas file-based) |
| 3 | **Linux** | ✅ | `Captures/plateforme_linux.png` (29/07, 15h38, **81,6 FPS**, HUD lu) **+ ton témoignage du 02/09** | **re-test sous WSL** — le vert date du 29/07 |
| 4 | **Web** | 🟡 | `Captures/plateforme_web_2026-09-02.png`, HUD lu, `Draw:1093 Tris:489586` | **une exécution sur GPU réel** — celle-ci est en **rendu logiciel** |
| 5 | **HarmonyOS** | ❔ | **jamais exercée en 3D** — sa seule capture (29/07 19h09) montre la **démo 0** | **reconstruire le `.hap`**, l'installer, lire le HUD |
| 6 | **macOS** | ❔ | **aucune trace** pour le chemin 3D | un build Metal, puis un Mac pour l'exécuter |
| 7 | **iOS** | ❔ | **aucune trace** pour le chemin 3D | idem + un appareil ou un simulateur |

### Ce qu'il faut, cible par cible — **exactement**

**3. LINUX — re-tester sous WSL** *(point C)*
Le vert repose sur une capture du **29/07** ; le build d'aujourd'hui n'y a jamais
tourné, donc **une régression depuis juillet resterait invisible**. Bloqué par la
machine, pas par le code : `wsl.exe --list --verbose` puis
`wsl.exe -e bash -lc 'uname -sr'` **n'ont rien rendu en 120 s** pendant cette
session. *Un WSL qui pend au-delà de deux minutes est un résultat.*
→ **Toi** : débloquer WSL2. → **Moi** : rebâtir `renderdemo` sous Linux, relancer
`--demo=2`, comparer le HUD à la capture du 29/07.

**4. WEB — confirmer sur un vrai GPU** *(point A, ~2 min)*
Tout le reste est fait : 30/30 à la construction, l'édition de liens passe,
l'initialisation est complète, **22 passes par image**, **zéro erreur** au
journal, et l'image sort en rendu logiciel. Il manque **une carte**.
Mode d'emploi complet au **bloc 1** (⚠️ port explicite `9002` : un serveur tourne
déjà sur `9001` et sert le **Debug** — sans le port, tu croirais tester le
Release).

**5. HARMONYOS — reconstruire le `.hap`** *(point B)*
🔴 **Ce n'est pas un échec, c'est un non-test.** La capture du 29/07 montre la
démo 0 ; le correctif `a762bda8` — *« demo 3D par defaut »*, `NK_DEFAULT_DEMO=2`
— date du **09/08 23h33**, **onze jours plus tard**. La capture et ton souvenir
parlent de **deux binaires différents**.

```
jenga build --target renderdemo --platform HarmonyOS --config Release
```
puis installer le `.hap` produit sous `Build/Bin/Release-HarmonyOS/renderdemo/`
sur l'émulateur (`C:\ohos\Emulator\…`, port hdc 5555) et lancer. **Tu n'as plus
rien à sélectionner** : la démo 3D est figée à la compilation depuis le 09/08.
⚠️ Purger `harmony-build/entry/build` avant repackage, sinon hvigor re-empaquette
l'ancienne `.so`.

🔵 **Une piste à 5 minutes, trouvée en rangeant — et je la borne.** Un `.hap`
`renderdemo` **existe déjà** :
`Nkentseu-nkcode/Build/Bin/Debug-HarmonyOS/renderdemo/renderdemo.hap`, **47 Mo,
daté du 10/08 20h41** — soit **21 h après** le correctif « démo 3D par défaut »,
et sur une branche (`feat/nkcode-panneaux`) dont l'historique **contient** ce
commit (vérifié : `a762bda8` en est bien un ancêtre).
⚠️ **Ce n'est PAS un verdict sur le moteur d'aujourd'hui** : ce binaire a
**23 jours de retard** sur les sources — tout le chantier Noge, les correctifs
Web, `quality`, le squelette lui sont postérieurs. *Un binaire plus vieux que ses
sources ne juge rien.* Il peut répondre à **une seule** question, gratuitement :
**la démo 3D démarre-t-elle par défaut sur HarmonyOS ?** Si oui, la
reconstruction est une formalité ; si non, on sait où chercher avant de payer le
build.

**6 et 7. macOS et iOS — la vraie carte n'est pas vide** *(hors mandat, non lancé)*
« Aucune trace » est vrai **pour le chemin 3D de `renderdemo`**. Mais il existe
déjà un chemin praticable, et il a **tourné** :
- `.github/workflows/build-remote.yml` construit **n'importe quelle cible** sur
  macOS ou iOS via un runner GitHub (`macos-latest`), en une commande :
  `gh workflow run build-remote.yml -f target=renderdemo -f platform=macos` ;
- il a **produit des artefacts réels le 28/08** :
  `Build/Artefacts-CI/GemCrush-macOS/GemCrush-macOS.tar.gz` (1,7 Mo) et
  `GemCrush-iOS/GemCrush-iOS.tar.gz` (1,3 Mo).

⚠️ **Ce que ça donne, et ce que ça ne donne pas.** GemCrush est un jeu **2D** :
il prouve la chaîne de build Apple, **pas** le chemin 3D. Et **la CI construit,
elle n'exécute pas** — l'artefact macOS demande un Mac pour tourner, l'iOS un
appareil ou un simulateur. Enfin, le backend **Metal « compile », il n'est pas
validé** (le wiki le documente en *intention*), et **NkSL n'a pas encore Metal**.

**L'ordre le moins cher, si tu veux ouvrir Apple** : (a) lancer le workflow sur
`renderdemo`/macos — ça coûte une commande et ça dit tout de suite si ça
**construit et lie** ; (b) seulement ensuite, chercher un Mac pour l'exécuter.
🚫 **Rien de tout ça n'a été lancé** : c'est hors du mandat de rangement. C'est
une carte, pas un travail commencé.

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
- **`NkSkeleton`** : **77 064 → 88 octets**, actif partagé façon `USkeleton`,
  tous les consommateurs migrés et re-exécutés verts.
- **`NKAnimation` → `NKAnima`** : 48 fichiers, 118 occurrences, `git mv`, **aucun
  espace de noms touché** (`nkentseu::anim`), tous les bancs au même compte
  (Noge 41/41, Nogee 45/45, LocomotionDemo 9/0, AssetIODemo 55/0,
  SystemsRevivalTest 34/0, NkAnimPhysTest 27/27).
- **Le document égaré est rangé** : `Applications/NkAnima/ROADMAP.md` →
  `Kernel/Runtime/NKAnima/ROADMAP_PRODUIT.md`, 10 renvois suivis, dossier vide
  retiré après vérification du registre de projets, **zéro ligne perdue**.
- **`*.nksl text eol=lf`** : les 131 shaders ont enfin une règle de fin de ligne.
- **`--demo 2` et `demo 2` ne retombent plus en silence sur la démo 0** — et la
  vraie cause du symptôme était le **répertoire courant** (2 erreurs en démo 0,
  **47 en démo 3D dont 18 shaders introuvables** : les deux chemins n'ont pas la
  même sensibilité à la même ressource manquante).
