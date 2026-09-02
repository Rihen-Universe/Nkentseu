# Noge — ce qui attend une décision de toi

> Une page. Le détail est dans `Engine/Noge/echanges_noge.md` (rapport complet).
> Branche `feat/noge-inventaire`, arbre `Nkentseu-noge`.
> **Mise en ordre du 2026-09-02, en fin de journée.**

## 📌 CE QUI RESTE, ET CE QUI NE T'ATTEND PLUS

**Cinq des huit blocs sont tranchés.** Ils restent écrits, avec leur réponse et
sa date — *une feuille qui garde des questions déjà répondues fait perdre du
temps ; une feuille qui efface l'historique fait re-trancher.*

### 🟠 CE QUI T'ATTEND ENCORE — **trois choses**, le point B est tombé ce soir

| # | ce qu'il faut | de qui | coût |
|---|---|---|---|
| ~~**A**~~ → **A′** 🔴 | ✅ **FAIT, et ça a RÉFUTÉ le vert Web.** Tu as lancé sur ta carte : `PBR` ne se lie pas — **17 unités de texture demandées, 16 accordées**, écran vide. Le vert d'hier venait de SwiftShader, plus permissif que le matériel. **Ce qui t'attend maintenant, ce n'est plus un test, c'est une décision** : lancer la **variante réduite de `PBR`** (conçue, chiffrée **~2-3 j**, non codée). ⚠️ macOS/iOS étant bloquées par la signature, **ce défaut coûte 3 plateformes sur 7**. Tout en **section 10**. | **toi** — dire quand | ~2-3 j |
| ~~**B**~~ | ✅ **FAIT le 02/09 à 19h31 — HarmonyOS REND LA 3D.** Tu as lancé l'émulateur, j'ai installé le `.hap` du 10/08 et **lu le HUD moi-même** : `Demo 3D | API : OpenGL`, panneau `Shadow tweak`, `FPS approx : 8.3`, 17 sphères PBR + ombres portées. **5 cibles sur 7.** ⚠️ Réserve écrite : binaire du **10/08**, donc l'image prouve « HarmonyOS rendait la 3D le 10/08 » — un re-test sur un `.hap` à jour reste à faire, **comme pour Linux**. Détail : **carte, section 9**. | — | fait |
| **C** | **Re-tester Linux sous WSL.** Le vert repose sur la capture du 29/07 + ton témoignage ; le build d'aujourd'hui n'y a jamais tourné. WSL2 n'a pas répondu en 120 s pendant cette session. | toi (débloquer WSL), puis moi | ~10 min |
| **E** | 🦴 **Faut-il UNIFIER les deux conventions de pose de repos ?** `NkRetargetSkeleton` (local relatif au parent) contre `NkSkeletonDef` (matrices bind/inverse-bind). Mesure faite : **ce ne sont pas deux versions d'une même chose**. Coûts, apports et recommandation au **bloc 11**. | **toi** | une phrase |
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
🗺️ **État au 02/09 au soir : 4 cibles vertes sur matériel réel** — Windows,
Android, Linux, **HarmonyOS (ce soir)**.
🔴 **Le Web a été RÉFUTÉ sur ta vraie carte** : `PBR` demande **17** unités de
texture, WebGL2 en accorde **16** — pas de PBR, écran vide. Le vert d'hier venait
d'un **rendu logiciel**, plus permissif que le matériel. **Section 10** : mesure,
datation (le shader a franchi la limite le **11/08 à 00h01**), doublure conçue,
banc spécifié.
🔒 **macOS et iOS ne sont pas en retard : elles sont BLOQUÉES PAR LA SIGNATURE DE
CODE.** La CI **construit** les deux, personne ne peut **exécuter** l'artefact.
Ce n'est pas un correctif moteur, c'est un compte développeur Apple.

> 🔴 **CONSÉQUENCE QUI CHANGE UNE PRIORITÉ — le Web EST le chemin Apple.**
> Tant que la signature bloque, c'est **par le navigateur** qu'un utilisateur
> macOS ou iOS verra tourner le moteur.
> ⚠️ **Et c'est précisément ce chemin-là qui vient d'être réfuté** (section 10).
> Le défaut `PBR`/16 unités ne bloque donc pas une cible sur sept : il bloque
> **la seule voie ouverte vers trois d'entre elles**. C'est ce qui en fait le
> point le plus rentable de tout le dossier.

---

## 1. 🔴 L'IMAGE WEB — **RÉFUTÉE SUR VRAI GPU** le 02/09 au soir

> ### 🔴 LE VERT DE CE BLOC EST TOMBÉ — lis la section 10 avant ce qui suit
> Sur ta vraie carte : `FRAGMENT shader texture image units count exceeds
> MAX_TEXTURE_IMAGE_UNITS(16)` → `PBR` ne se lie pas → **écran vide**.
> Mesure : `pbr.frag.nksl` déclare **27** échantillonneurs, la fusion des cookies
> en retire **10**, il en reste **17** pour **16** accordées. **Une de trop.**
> Le shader a franchi la limite le **11/08 à 00h01** et personne ne l'a su
> pendant 22 jours, parce que le seul web jamais exécuté (SwiftShader) en accorde
> plus de 16.
>
> 📌 **Le texte ci-dessous n'est pas effacé** : la capture était vraie, la chaîne
> logicielle est bel et bien correcte, et le mode d'emploi reste valable pour le
> jour où la variante réduite sera là. *On ne corrige pas un journal, on le date.*

> ### 🟡 CE QUI AVAIT ÉTÉ ÉCRIT LE MATIN — vrai en logiciel, réfuté sur matériel
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
>
> 🔴 **ET CE BLOC A CHANGÉ DE POIDS LE MÊME SOIR.** Tu as nommé la cause du
> blocage Apple — *« ça compile avec GitHub Actions mais je ne peux pas exécuter
> à cause de la signature, donc il faut que le web fonctionne »*. **Le navigateur
> est donc le chemin par lequel macOS et iOS verront tourner le moteur.** Ces
> deux minutes ne confirment plus une case : elles ouvrent **trois plateformes
> sur sept**. Détail en section 9, points 6 et 7.

La chaîne 3D web est **débloquée et vérifiée** : elle construit (30/30), elle lie,
elle initialise entièrement, elle exécute 22 passes par image, et il n'y a **plus
une seule erreur** dans le journal. Ce qui manque n'est pas du code : c'est **un
vrai GPU**. Je n'ai que SwiftShader (rendu logiciel), trop lent pour aboutir, et
je ne touche pas à ta carte pendant qu'Ilyana s'entraîne.

> # 🚨 9002. PAS 9001.
> ### La consigne était déjà écrite plus bas, et elle a raté : tu as testé sur **9001**, donc le **Debug**.
> On l'a su à tes `[WebDiag]` qui inondaient la console — ils sont **éteints en
> Release**. Le défaut `PBR` est **indépendant du build** (il aurait rougi
> pareil), donc **ton verdict tient**. Mais la prochaine mesure ne doit pas se
> jouer là-dessus : une consigne enterrée sous vingt lignes n'est pas une
> consigne, c'est une note d'espoir.
>
> ```
> Build\Bin\Release-Web\renderdemo\renderdemo.bat 9002
> http://localhost:9002/renderdemo.html?demo=2
> ```
>
> **Le témoin qui tranche en une ligne** — la taille du wasm :
> `curl -s -o NUL -w "%{size_download}\n" http://localhost:9002/renderdemo.wasm`
> **27 444 289 = Release ✅** · **35 052 605 = Debug ❌ (tu es sur l'ancien serveur)**
>
> 📌 *Le piège n'est pas d'oublier le port : c'est que `renderdemo.bat` prend
> **9001 par défaut**, échoue à réserver le port **sans le dire**, et ouvre quand
> même ton navigateur — sur le serveur de quelqu'un d'autre. **Un échec muet qui
> ouvre quand même une fenêtre est pire qu'un échec** : il fabrique un faux
> témoin qui a l'air juste.*

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

## 9. 🗺️ LA CARTE DES 7 PLATEFORMES — **4 vertes sur matériel réel**, et ce qu'il faut pour les 3 autres

> 🔄 **Compte révisé le 02/09 au soir, à la baisse.** On a dit « 5 sur 7 » pendant
> quelques heures : le Web y était compté sur une image obtenue en **rendu
> logiciel**. Le test sur ta vraie carte l'a **réfuté** — `PBR` ne se lie pas
> (17 unités de texture demandées, 16 accordées). **Section 10.**
> Vertes sur matériel réel : **Windows, Android, Linux, HarmonyOS**.

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
>
> 📦 **LES PREUVES SONT MAINTENANT DANS LE DÉPÔT — et la règle qui les y garde.**
> Jusqu'au 02/09, toutes les images citées ici vivaient **hors dépôt**
> (`/Captures/` était ignoré en bloc) : *un verdict dont la preuve peut
> disparaître d'un coup de ménage n'est pas un verdict, c'est un souvenir.*
> Décision de Rodolf, avec sa contrainte — *« retire donc Captures de gitignore,
> mais je dois limiter le nombre de fichiers lourds »* :
>
> > **Une image qui fait foi dans un document est versionnée.
> > Une image de travail ne l'est pas.**
>
> Appliquée en **allowlist nommée** dans `.gitignore` (pas un dossier entier) :
> **9 fichiers, 2 292 069 octets** — les 5 captures de plateforme, les **2 témoins
> invalidés** qu'on garde exprès pour pouvoir raconter les deux verdicts qu'ils
> ont fait tomber, et les 2 captures de `model_loaders/` réellement citées. Les
> 941 autres images du dossier restent ignorées. Détail, justification par image
> et **contrôle négatif** : **`Captures/LISEZMOI.md`**.

| # | cible | 3D | preuve | ce qui manque |
|---|---|---|---|---|
| 1 | **Windows** | ✅ | backend de référence, `plateforme_windows.png` (29/07, 142,1 FPS) | — |
| 2 | **Android** | ✅ | `Captures/nk_android_demo3d.png` — 18 sphères PBR, ombres, **59 FPS**, `VSM atlas 4096 px` | — (⚠️ textures **procédurales**, pas file-based) |
| 3 | **Linux** | ✅ | `Captures/plateforme_linux.png` (29/07, 15h38, **81,6 FPS**, HUD lu) **+ ton témoignage du 02/09** | **re-test sous WSL** — le vert date du 29/07 |
| 4 | **Web** | 🔴 | **ROUGE sur GPU réel** (02/09 au soir) : `PBR` ne se lie pas — `texture image units count exceeds MAX_TEXTURE_IMAGE_UNITS(16)`, écran vide. 🟡 **Vert en LOGICIEL** : `Captures/plateforme_web_2026-09-02.png` reste vraie, prise sous SwiftShader qui accorde **plus de 16** unités | **la variante réduite de `PBR`** — 17 échantillonneurs demandés pour 16 accordés. **Section 10** |
| 5 | **HarmonyOS** | ✅ | `Captures/plateforme_harmonyos_2026-09-02.jpeg` — **HUD lu le 02/09 à 19h31** : `Demo 3D \| API : OpenGL`, panneau `Shadow tweak` (`VSM atlas 4096 px`), `FPS approx : 8.3`, 17 sphères PBR + ombres portées | **re-test sur un `.hap` à jour** — l'image vient du binaire du **10/08** |
| 6 | **macOS** | 🔒 | **ça construit** (CI GitHub, artefact réel le 28/08) — **ça ne s'exécute pas** | 🔒 **la SIGNATURE DE CODE**, pas un correctif moteur |
| 7 | **iOS** | 🔒 | idem — la CI produit un artefact, personne ne peut le lancer | 🔒 **signature + profil d'approvisionnement**, plus contraignant que macOS |

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

**5. HARMONYOS — ✅ VERT LE 2026-09-02 À 19h31** *(point B — fait)*

> # ✅ HARMONYOS REND LA 3D. J'AI OUVERT L'IMAGE ET LU LE HUD.
>
> **`Captures/plateforme_harmonyos_2026-09-02.jpeg`** — 2720×1260, prise sur
> l'émulateur que tu venais de lancer. Ce que le HUD dit, mot pour mot :
>
> ```
> Demo 3D  |  API : OpenGL  |  Affichage(Z): RENDERED  |  Couleur(B): GRIS
> FPS approx : 8.3  |  dt: 119.90 ms
> [Phase H] Texture file-based : fallback procedural
>
>          == Shadow tweak (panel debug) ==
>          VSM atlas : 4096 px      quality : 4
>          softness  : 0.005        slots : 14 (rend 1 | cache 13)
>          framesInFlight : 3
> ```
>
> | ce que je cherchais | trouvé |
> |---|---|
> | ligne **`Demo 3D \| API :`** — *le* test | ✅ `Demo 3D | API : OpenGL` |
> | panneau **`Shadow tweak`** | ✅ monté, `VSM atlas 4096 px` |
> | **`FPS approx`** avec une valeur | ✅ `8.3` |
> | l'image | ✅ **17 sphères PBR**, **ombres portées douces** sous chacune, grille de cubes colorés, deux colonnes projetant leur ombre, grille verte d'instances, gizmo 3D, sol quadrillé |
>
> 🚫 **`Draw:1093  Tris:489582  Batches:1093` — non nuls, et ils n'ont PAS jugé.**
> Ils sont là parce que ce binaire (10/08) est postérieur au câblage des compteurs
> (05/08), exactement comme prévu. Le verdict tient sur `Demo 3D` + `Shadow tweak`.
> 📌 **Mais ils offrent un recoupement gratuit, et il est frappant** : le Web
> logiciel affichait **`Draw:1093 Tris:489586`**, HarmonyOS affiche
> **`Draw:1093 Tris:489582`**. Deux plateformes, deux backends, **le même nombre
> de dessins et quatre triangles d'écart**. C'est la même scène qui rend des deux
> côtés — un recoupement que ni l'une ni l'autre des deux mesures ne pouvait
> fabriquer seule.
>
> ⚠️ **LES BORNES, ET ELLES COMPTENT** :
> 1. **binaire du 10/08** → l'image prouve « **HarmonyOS rendait la 3D le 10/08** »,
>    pas « le moteur d'aujourd'hui y rend ». *Même forme que Linux, dont le vert
>    porte sur le 29/07.* Le re-test sur un `.hap` à jour reste à faire ;
> 2. **8,3 FPS n'est PAS un verdict de performance** : build **Debug**, sur un
>    émulateur dont le GL est paravirtualisé (`DGLES`), pendant qu'Ilyana occupe
>    la carte. Ce chiffre dit « ça tourne », rien de plus ;
> 3. **textures procédurales**, pas file-based (`[Phase H] fallback procedural`) —
>    exactement comme Android.
>
> 🧰 **Deux pièges d'instrument franchis, notés parce qu'ils reviendront** :
> - **`hdc list targets` rendait `[Empty]` alors que l'émulateur tournait.** Il
>   écoutait bien sur `127.0.0.1:5555` (vérifié au `netstat`), mais le serveur
>   `hdc` ne s'y était pas connecté : il faut **`hdc tconn 127.0.0.1:5555`**
>   d'abord. *Sans ce contrôle, l'échec d'installation qui aurait suivi se serait
>   lu comme un échec du binaire* — la cinquième fois de la journée qu'un problème
>   d'instrument imite un problème de code ;
> - **`hdc` préfixe le répertoire courant à un chemin absolu Windows**
>   (`d:\Rihen\Rodolf\D:/Projets/…`) et **Git Bash convertit les chemins distants**
>   `/data/local/tmp/…` en chemins Windows. Remèdes : se placer dans le dossier du
>   `.hap` et le nommer nu ; `MSYS_NO_PATHCONV=1` pour `file recv`. Et
>   `snapshot_display` **refuse `.png`** — le suffixe doit être `.jpeg`.
>
> 🟢 **Ilyana n'a jamais été touchée.** Relevés encadrant l'opération : avant
> **5 206 Mio / 97 %**, pendant **6 887 Mio / 50 %** (l'émulateur a pris ~1,4 Gio),
> après arrêt de l'application **5 922 Mio**, puis retour à **44-80 %**. Son
> compteur CPU a continué d'avancer (+1,22 s en 5 s). **J'ai arrêté MON
> application** (`aa force-stop`) dès la capture obtenue — pas l'émulateur, qui est
> le tien, et surtout pas l'entraînement.
>
> ⚠️ **La capture n'est PAS dans le commit** : `Captures/` est **gitignoré**
> (`.gitignore:689`), comme l'étaient déjà `plateforme_linux.png` et
> `plateforme_web_2026-09-02.png`. Toute la base de preuves de cette carte vit
> hors dépôt. Le fichier est sur le disque, dans `Nkentseu/Captures/`.

**Ce qui était écrit avant l'image — conservé, parce que c'est le raisonnement
qui a mené à la prendre :**

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

### 🗣️ TON TÉMOIGNAGE DU 2026-09-02 — *« oui »* — et ce qu'il ferme exactement

**Question posée** : la démo 3D démarre-t-elle par défaut sur HarmonyOS ?
**Ta réponse** : **oui**. Concordant avec ce que tu disais déjà le matin —
*« j'ai testé la démo `--demo=` ce jour-là, ça a fonctionné sur toutes les
plateformes sauf iOS et macOS »*, par le chemin **NKRHI / NKRenderer**.

✅ **Ce que ça ferme : LE CANAL DE SÉLECTION.** C'était la seule inconnue de
mécanisme, et elle est levée. Le code l'écrit noir sur blanc — `main.cpp:411` :
*« `NK_DEFAULT_DEMO` : démo de repli quand **AUCUN canal de sélection runtime
n'existe**. C'est le cas de HarmonyOS NEXT : le sandbox ne monte pas
`/data/local/tmp` (fichiers `kDemoFiles` invisibles, vérifié sur l'émulateur —
Permission denied même pour `hdc`, qui n'est pas root), et le NDK public n'expose
aucune API de paramètre système. »* Face à ça, `RendererSandbox.jenga:300` fige
`defines(["NK_DEFAULT_DEMO=2"])` dans le filtre HarmonyOS. **Ton « oui » dit que
ce figeage fait son travail** : on n'a plus à pousser de `nk_demo.txt`, ni à
générer un `EntryAbility.ets` pour relayer la démo par le Want d'`aa start`.

🔴 **CE QUE ÇA NE FAIT PAS : LA COLONNE NE BOUGE PAS. HarmonyOS reste ❔.**
Une cible passe au vert sur **une image ouverte et un HUD lu**, jamais sur un
souvenir — *le tien pas plus que le mien*. C'est cette règle exacte qui a payé
aujourd'hui : ta capture du 29/07 semblait prouver HarmonyOS, elle montrait la
démo 0, et **deux verdicts sont tombés** quand on l'a relue avec le bon témoin.
Ce que ton « oui » change, c'est le **pronostic**, pas la preuve.

> **État à écrire tel quel : témoignage concordant, image manquante.**

📌 **DÉPASSÉ UNE HEURE PLUS TARD — et la règle a tenu jusqu'au bout.** Rodolf a
lancé l'émulateur ; l'image a été prise, ouverte, son HUD lu. **La colonne bouge
maintenant, et c'est l'image qui la bouge, pas le témoignage.** Les deux
concordaient : c'est agréable, ça n'a jamais été une preuve. *Le paragraphe
ci-dessus reste écrit parce qu'il dit pourquoi on est allé chercher l'image au
lieu de se contenter d'un « oui ».*

⚠️ **Et même l'image, quand elle viendra de ce `.hap`-là, sera bornée** : elle
prouverait « **HarmonyOS rendait la 3D le 10/08** », pas « le moteur d'aujourd'hui
y rend ». Exactement le statut de Linux, dont le vert porte sur le **29/07**.

### 🕐 ⛔ POURQUOI JE NE PRENAIS PAS LA CAPTURE — *levé à 19h20, Rodolf a lancé l'émulateur*

> **Ce blocage est résolu** : Rodolf a lancé l'émulateur lui-même, la capture a
> été prise dans la foulée (résultat en tête de ce bloc). Le texte reste parce
> qu'il documente la règle de priorité, qui, elle, ne change pas.

**Aucun appareil connecté** : `hdc list targets` rend **`[Empty]`** — l'émulateur
n'est pas lancé, il faudrait le démarrer.
⚠️ **Et ce même `[Empty]` s'est reproduit APRÈS le lancement** : l'émulateur
écoutait, mais `hdc` n'y était pas connecté. Le remède est `hdc tconn
127.0.0.1:5555` — voir les pièges d'instrument en tête de bloc.

**Et c'est là que ça bloque, en une ligne : le démarrer prendrait le GPU
d'Ilyana.** Mesure du moment : **RTX 3070 Laptop, utilisation 100 %,
5 206 / 8 192 Mio occupés** (`NKIlyana`, PID 19692, en entraînement depuis le
01/09 18h08). L'émulateur HarmonyOS NEXT est un système complet avec accélération
graphique matérielle — il entrerait en concurrence directe sur une carte déjà
saturée, avec moins de 3 Gio libres, et il ouvrirait sa fenêtre sur ta session.
**Ilyana est prioritaire : on ne l'arrête pas, on ne lui dispute pas sa carte.**

📌 *Ce n'est pas un obstacle technique, c'est un ordre de priorité.* La capture
elle-même serait légitime — `hdc shell snapshot_display` photographie l'écran de
**l'émulateur**, jamais le tien.

**La séquence — ✅ CELLE-CI A MARCHÉ, corrigée des trois pièges rencontrés.**
À réutiliser telle quelle pour le re-test sur un `.hap` à jour (~10 min) :

```sh
export PATH="$PATH:/c/ohos/Emulator/HarmonyOS-NEXT-miku404/sdk/HarmonyOS-NEXT-DB1/openharmony/toolchains"
export MSYS_NO_PATHCONV=1          # sinon Git Bash traduit /data/local/tmp en chemin Windows

hdc tconn 127.0.0.1:5555           # ⚠️ INDISPENSABLE : sans ça, list targets rend [Empty]
hdc list targets                   # doit afficher 127.0.0.1:5555

cd .../Debug-HarmonyOS/renderdemo  # ⚠️ hdc préfixe le cwd à un chemin absolu Windows
hdc -t 127.0.0.1:5555 install renderdemo.hap

hdc -t 127.0.0.1:5555 shell "aa start -a EntryAbility -b com.nkentseu.sandbox.render.demo -m entry"
hdc -t 127.0.0.1:5555 shell "hilog -P <pid>"        # le journal dit pourquoi s'il n'y a pas d'image
hdc -t 127.0.0.1:5555 shell "snapshot_display -f /data/local/tmp/harmony_3d.jpeg"   # ⚠️ .jpeg, pas .png
hdc -t 127.0.0.1:5555 file recv /data/local/tmp/harmony_3d.jpeg plateforme_harmonyos_<date>.jpeg

hdc -t 127.0.0.1:5555 shell "aa force-stop com.nkentseu.sandbox.render.demo"   # rendre la carte
```

📌 **Le journal, relevé pendant l'exécution, confirme l'image sans la remplacer** :
`eglSwapBuffers` toutes les ~1 s avec **53 000 à 80 000 appels GL par lot**. Une
charge pareille n'est pas celle d'un écran vide — mais c'est un **indice**, et
c'est le HUD qui a tranché.

**Ce que je lirai dans l'image — et rien d'autre** : la ligne
**`Demo 3D | API : …`**, le panneau **`== Shadow tweak ==`**, un **`FPS approx`**
avec une valeur. 🚫 **Pas `Draw:` / `Tris:`** — ce binaire est du 10/08, donc
postérieur au câblage des compteurs du 05/08 : ils seront probablement non nuls,
**et ça ne prouvera rien**. L'échec, lui, a une signature nette : un aplat uni
avec seulement `Active: R2D|R3D|TEXT|OVERLAY` et **aucune** ligne `Demo 3D`.

✅ **Ce critère a été écrit AVANT de regarder l'image, et appliqué tel quel.**
Les trois marqueurs étaient là ; `Draw:`/`Tris:` étaient non nuls comme annoncé
et n'ont pas servi. *Un critère de verdict posé avant la mesure est le seul qui
ne s'ajuste pas au résultat.*

**6 et 7. macOS et iOS — 🔒 BLOQUÉES PAR LA SIGNATURE, pas par le moteur**

> 🗣️ **Rodolf, 2026-09-02** : *« pour macOS et iOS je ne peux pas tester pour
> l'instant, car ça compile avec GitHub Actions mais je ne peux pas exécuter à
> cause de la signature — donc il faut que le web fonctionne. »*

📌 **La cause est nommée, et ça change la nature de la case.** Ces deux cibles ne
sont pas « sans trace faute d'avoir essayé » : **elles construisent**, l'artefact
existe, et **c'est l'exécution qui est interdite**. macOS refuse de lancer un
binaire non signé et non notarisé (Gatekeeper) ; iOS n'installe rien sans
signature **et** profil d'approvisionnement. *Un blocage administratif ressemble
à un trou technique sur un tableau — il ne se répare pas du tout pareil.*

**Ce qu'il faudrait exactement, pour que le coût soit déjà chiffré le jour où tu
voudras débloquer :**

| | macOS | iOS |
|---|---|---|
| **compte** | Apple Developer Program — **99 $/an** | le même compte, obligatoire |
| **identité** | certificat *Developer ID Application* | certificat *Apple Development* / *Distribution* |
| **en plus** | **notarisation** (envoi à Apple, puis agrafage) pour lancer hors du Mac de compilation | **profil d'approvisionnement** listant l'UDID de chaque appareil |
| **matériel pour exécuter** | un Mac | un iPhone/iPad, **ou** le simulateur (qui, lui, ne demande pas de signature) |
| **contourne la signature ?** | oui, localement : `xattr -d com.apple.quarantine`, ou lancer sur le Mac qui a compilé | **non** sur appareil réel — seul le **simulateur** échappe à la contrainte |

🔵 **Les deux chemins les moins chers, dans l'ordre, si tu veux ouvrir Apple** :
1. **le simulateur iOS** — il exécute sans signature. Il faut un Mac (le runner CI
   en est un, mais il rend un artefact, il ne rend pas d'écran) ;
2. **un Mac emprunté une heure**, pour lancer l'artefact macOS déjà produit avec
   `xattr -d com.apple.quarantine`. Pas de compte, pas de notarisation, juste une
   image à regarder.

*Aucun des deux ne demande les 99 $ — ils demandent un Mac. Le compte ne devient
obligatoire que pour **distribuer**, pas pour voir tourner.*

**Et ce qui est déjà là, côté construction** — ça a tourné :
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
**construit et lie** ; (b) seulement ensuite, trouver de quoi l'exécuter (Mac
emprunté, ou simulateur iOS).
🚫 **Rien de tout ça n'a été lancé** : c'est hors du mandat de rangement. C'est
une carte, pas un travail commencé.

### 🔴 CE QUE LE BLOCAGE APPLE CHANGE POUR LE WEB — une priorité, pas une remarque

Rodolf le dit lui-même en une clause : *« …donc il faut que le web fonctionne. »*

**Tant que la signature bloque, le navigateur EST le chemin Apple.** Un
utilisateur macOS ou iOS ne verra pas tourner Noge par un binaire natif — il le
verra par le Web. Ce qui reclasse le **point A** :

| avant | après |
|---|---|
| « confirmer la 7ᵉ case, 2 minutes » | **la seule voie ouverte vers 2 plateformes sur 7** |

⚠️ **Et ça déplace aussi une réserve déjà écrite.** Le vert Web est en rendu
**logiciel** ; les trois doublures CPU (cloth/hair/softbody) sont **absentes du
Web** parce que WebGL2 n'a pas de compute (bloc 2, classé « après la course »).
Cette décision reste bonne pour la course — mais **si le Web devient la vitrine
Apple, ces absences ne concernent plus une cible sur sept : elles concernent
trois**. Ce n'est pas une raison de rouvrir le bloc 2 maintenant ; c'est une
raison de le **relire le jour où Apple comptera**.

---

## 10. 🔴 LE DÉFAUT WEB SUR VRAI GPU — `PBR` demande **17** unités de texture, la cible en donne **16**

> 🗣️ **Ton verdict, 2026-09-02 au soir**, sur ta vraie carte :
> ```
> [NkRHI_GL][WebDiag] link FAIL:
> FRAGMENT shader texture image units count exceeds MAX_TEXTURE_IMAGE_UNITS(16)
> [NkShader] CreateShader fail 'PBR' (glslang : V:1 F:1)
> ```
> Pas de PBR → pas de sphères → **écran vide**. Le Web n'est **pas** vert.

### 🔑 Ce que ça fait à la capture d'hier — réinterprétée, pas annulée

`Captures/plateforme_web_2026-09-02.png` **était vraie**. Elle a été prise sous
**SwiftShader**, qui annonce **plus de 16** unités. Elle montrait donc une chaîne
3D correcte… dans un environnement **plus permissif que le matériel**.

> 🔴 **LA LEÇON, ET ELLE VAUT BIEN AU-DELÀ DU WEB :**
> **un rendu logiciel valide la LOGIQUE et masque les LIMITES MATÉRIELLES** —
> unités de texture, tailles d'uniformes, formats, extensions, précisions. Un
> vert obtenu en logiciel ne se note donc jamais « ça marche », mais
> **« logique validée, matériel non éprouvé »**.
>
> C'est la famille du jour, encore : *le témoin ne varie pas comme le sujet.* Le
> logiciel est un juge **plus généreux** que le réel — et un juge généreux ne dit
> rien quand on franchit une limite.

⚠️ **Et le coût n'est plus d'une cible.** macOS et iOS étant bloquées par la
signature, **le navigateur est le chemin Apple**. Ce défaut ne coûte donc pas une
plateforme : **il en coûte trois sur sept.**

### 📏 LA MESURE — le compte exact, et il tombe à UN près

`Resources/NKRenderer/Shaders/PBR/NkSL/pbr.frag.nksl` déclare **27
échantillonneurs** dans l'étage fragment :

| famille | n | noms |
|---|---:|---|
| matériau | **5** | `tAlbedo` `tNormal` `tORM` `tEmissive` `tHeight` |
| IBL | **3** | `tEnvIrradiance` `tEnvPrefilter` `tBRDFLUT` |
| ciel / AO volumétrique | **2** | `tSkyEnvCube` `tVoxelOpacity` |
| ombres | **2** | `tShadowAtlas` `tShadowAtlasRaw` |
| cookies 2D | **8** | `tLight3DCookie0..7` |
| cookies cube | **4** | `tLight3DCubeCookie0..3` |
| divers | **3** | `tMatcap` `tLTC1` `tLTC2` |
| **total** | **27** | |

**Une doublure existe déjà** — `NkWebMergeCookieSamplers`
(`NkOpenglDevice.cpp:2013`) : elle **supprime les déclarations** des cookies
`1..7` et cube `1..3`, et redirige leurs usages vers le slot 0. Elle en retire
**10**.

```
27 déclarés  −  10 fusionnés  =  17 actifs        la cible en donne 16
                                                  →  il en manque UNE
```

🔴 **Le commentaire du code annonce « 24 → 14 ». Il avait raison le jour où il a
été écrit.** L'inventaire qu'il énumère compte 24 samplers — il **ne connaît pas**
`tHeight`, `tLTC1`, `tLTC2`. Datation au `git log -S` :

| date | événement | total | après fusion |
|---|---|---:|---:|
| **31/07** | la fusion est écrite (`22b030b6`) | 24 | **14** ✅ |
| **10/08 22h53** | `tHeight` — parallax occlusion (`b45aba13`) | 25 | 15 ✅ |
| **11/08 00h01** | `tLTC1` + `tLTC2` — tables LTC (`8e72961a`) | **27** | **17** 🔴 |

> **Le shader a franchi la limite le 11 août à 00h01, et personne ne l'a su
> pendant 22 jours** — parce que le seul environnement web jamais exécuté
> (SwiftShader) en accorde plus de 16. *Un budget calculé une fois, dans un
> commentaire, n'est pas un budget : c'est le souvenir d'un budget.* Rien ne le
> recalculait quand le shader grossissait.

✅ **Contrôle d'étendue — `PBR` est le SEUL au-dessus de 16.** Relevé sur les 25
étages fragment du dépôt : Terrain 9, Water 6, CarPaint 6, Skin 5, Glass 5,
SSR 4, tout le reste ≤ 3. **Le défaut est localisé, pas systémique** — bonne
nouvelle pour le coût du correctif.

### 📌 L'IRONIE UTILE — le moteur ne connaît pas sa propre limite

Sur ce même chemin, `QueryCaps` rendait **« caps non disponibles »**
(`NkDeviceFactory.cpp:132`). Mais le vrai problème est un cran plus bas, et il se
mesure :

> **`NkDeviceCaps` porte 16 champs `max*` — et AUCUN ne dit combien d'unités de
> texture l'étage fragment peut adresser.**
> Il connaît `maxTextureDim2D`, `maxTextureArrayLayers`, `maxColorAttachments`,
> `maxVertexAttributes`, `maxSamplerAnisotropy`… **la seule limite qui a cassé la
> cible est la seule qu'il ne porte pas.**

**Donc le moteur ne peut pas s'y adapter, même s'il le voulait** — et c'est
exactement ce que la règle gravée exige : *le jeu déclare une intention, le
moteur décide.* Ici, le moteur **n'a pas la donnée pour décider**. Il ne peut que
subir, en silence, jusqu'au `glLinkProgram`.

*Parente de « ne pas poser la question vaut mieux que traiter l'erreur », prise à
l'envers : on ne pose pas la question ET on ne traite pas l'erreur — on découvre
la limite en la franchissant, chez toi, dans ta console.*

### 🛠️ LA DOUBLURE — conçue, **non codée** (trop grosse pour un lot de rangement)

**Le principe, non négociable** : *ce qui ne peut pas se faire doit avoir une
doublure crédible, jamais un trou.* Une cible à 16 unités doit obtenir une
**variante réduite de PBR**, choisie **par le moteur**.

🚫 **Aucun `#ifdef WEB` dans le shader.** Ce serait la plateforme qui remonte
dans le contenu — l'inverse exact de la règle. Le shader ne sait jamais où il
tourne ; il connaît un **budget**, que le moteur lui donne.

**Étape 0 — ✅ FAITE le 02/09 : la limite est désormais CONNAISSABLE**
Ajouter `maxFragmentTextureUnits` à `NkDeviceCaps`, renseigné par chaque backend
(`glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS)` côté GL). ⚠️ **Avec la garde déjà
gravée** : `glGetIntegerv` **n'écrit rien** quand il échoue — valeur de repli
**décidée** (16, le minimum garanti par WebGL2), jamais un zéro par défaut, et
jamais la valeur de la variable voisine.

> ✅ **FAIT et compilé (NKRHI 16/16)** : `maxFragmentTextureUnits = 16` ajouté à
> `NkDeviceCaps` (`NkIDevice.h`), renseigné par
> `NkGLQueryCap(GL_MAX_TEXTURE_IMAGE_UNITS)` dans `NkOpenglDevice::QueryCaps`.
> **Le défaut de 16 n'est pas un zéro par défaut, c'est une valeur décidée** : si
> la requête échoue, `NkGLQueryCap` n'écrit rien et le champ garde **la valeur la
> plus contraignante**, jamais la plus optimiste — et jamais la valeur de la
> variable voisine `v`, qui est exactement le défaut des « sept capacités fausses
> et plausibles » corrigé plus bas dans le même fichier.
> 📌 *La structure portait 16 limites et aucune n'était celle-ci. C'est
> maintenant 17, et le moteur peut enfin poser la question avant de choisir sa
> variante.*

**Étape 1 — un palier « unités de texture » dans `NkRenderQuality`**
Le profil porte le budget ; `ForTarget()` le renseigne depuis les caps. Le
matériau demande des canaux, le moteur en accorde autant que le budget permet.

**Étape 2 — la variante réduite. 🔴 RÈGLE : FUSIONNER AVANT D'ÉTEINDRE.**

> 🗣️ **Rodolf, 02/09** : *« s'il y avait possibilité de fusionner certaines
> textures, ça devrait être vraiment bien pour éviter de perdre en qualité. »*

**Les six unités récupérables ne sont pas de même nature, et c'est tout le
sujet** — quatre ne coûtent rien, deux coûtent une perte visible :

| nature | ce que ça fait | perte |
|---|---|---|
| **FUSION** (4 unités) | la donnée est **toujours là**, simplement rangée autrement — deux vues d'une même texture, deux LUT dans un atlas, un cube réutilisé | **aucune, ou négligeable** |
| **EXTINCTION** (2 unités) | la donnée **disparaît**, remplacée par un repli | **visible** |

> **L'ordre d'application est une RÈGLE, pas une préférence : on épuise la
> fusion avant de toucher à l'extinction.**

✅ **ET LA FUSION SUFFIT. On n'éteint RIEN.** Compte mesuré :

```
27 déclarés − 10 (fusion des cookies) − 4 (les quatre fusions) = 13  ≤ 16 ✅
```

**13 sur 16, zéro extinction, 3 unités de marge.** Les deux extinctions
(`tVoxelOpacity`, `tMatcap`) **restent en réserve, non appliquées** — elles ne
serviront que si une cible future descend sous 13.

📌 **Et c'est mieux que la marge de 11 que je visais.** *13 sans extinction vaut
mieux que 11 avec* : la perte est visible, la marge ne l'est pas. On ne paie pas
en qualité une sécurité qu'on peut obtenir en rangement.

### 🔬 ÉTAPE 2 — LA MESURE A CORRIGÉ MA CONCEPTION : un seul sampler suffit

⚠️ **J'avais conçu quatre fusions avant d'aller lire le shader. En le lisant,
deux d'entre elles se sont révélées inutiles ou fausses.** Ce que la lecture
donne, sampler par sampler :

| candidat | ce que la lecture montre | verdict |
|---|---|---|
| **`tShadowAtlasRaw`** | **1 seul usage**, dans la recherche de bloqueurs **PCSS** (`NkShadowAtlas.glsli:228`). Or `ApplyQuality()` met `pcss = false` en `NK_MOBILE` et `NK_LOW` — et `ForTarget()` donne `NK_MOBILE` au Web. **Sur la cible contrainte, ce sampler est déjà dans une branche morte.** | ✅ **GRATUIT** — rien à fusionner, rien à éteindre : la fonctionnalité est déjà désactivée par le palier de qualité |
| `tVoxelOpacity` | 🔴 **je l'avais classé « jamais échantillonné » — c'était FAUX.** Le `grep` sur `pbr.frag.nksl` ne montrait que sa déclaration ; il est échantillonné dans `Include/NkVoxelAO.glsli:57`, inclus ligne 168. | ❌ **pas libre** — l'éteindre reste une extinction, à garder en réserve |
| `tEnvIrradiance` + `tEnvPrefilter` | fusion réelle, change l'éclairage indirect | ⏳ **demande une capture A/B** |
| `tLTC1` + `tLTC2` | atlas 2D — le repliage des UV touche 5 sites | ⏳ **la plus coûteuse des quatre**, comme pressenti |
| `tSkyEnvCube` | réutiliser le cube IBL change le reflet miroir | ⏳ **demande une capture A/B** |

> 🔑 **`17 − 1 = 16 ≤ 16`. Le seul retrait gratuit suffit à faire tenir la
> cible** — sans fusionner quoi que ce soit, sans rien éteindre, sans toucher à
> l'éclairage. Marge : **0**.
>
> 📌 **Et c'est la leçon de méthode du lot** : *j'ai conçu quatre fusions avant
> de lire le shader ; la lecture en a rendu deux inutiles et une fausse.* La
> conception sur inventaire de noms est plus rapide que la lecture — et c'est
> exactement pour ça qu'elle se trompe.

**Ce qui reste à décider, et c'est à toi** : 16/16 tient, mais **sans marge** —
et c'est précisément l'état qui a produit le défaut du 11 août. Deux voies :

| | ce qu'on fait | résultat | prix |
|---|---|---|---|
| **a** | le retrait gratuit **seul** | **16/16**, marge 0 | aucun — livrable sans capture |
| **b** | + les fusions IBL et sky | **14/16**, marge 2 | 2 captures A/B, GPU requis |

🔵 **Ma recommandation : (a) maintenant, (b) quand le GPU sera libre.** *16 livré
sans risque vaut mieux que 14 promis* — et le banc, lui, est déjà là pour dire le
jour où le shader repassera au-dessus.

⛔ **Pourquoi je n'ai pas codé (a) ce soir, et c'est un blocage nommé** : le
retrait doit être **conditionnel au budget mesuré**, jamais inconditionnel — le
bureau garde ses 17 samplers et son PCSS. Or **NkSL n'a aucune injection de
`#define`** : `NkSLCompileOptions` (`NkSLTypes.h:419`) porte versions, modèles de
shader et drapeaux — **aucune macro**. Les deux mécanismes possibles :
1. **ajouter l'injection de macros à `NkSLCompileOptions`** — propre, réutilisable,
   mais c'est une évolution du langage, pas un correctif ;
2. **étendre la transformation de source qui existe déjà** —
   `NkWebGL2AdaptGLSL`/`NkWebMergeCookieSamplers` (`NkOpenglDevice.cpp:2013`)
   réécrit déjà la source pour la cible contrainte. Le retrait de
   `tShadowAtlasRaw` y tiendrait en quelques lignes, **juste à côté de la fusion
   des cookies qui est exactement la même famille**.

🔵 **La (2) est la bonne** — la porte de la maison le dit : *avant d'écrire un
mécanisme, chercher qui le porte déjà*. Le mécanisme existe, il est éprouvé, et
il est déjà branché sur ce chemin exact. ⚠️ Mais il est aujourd'hui piloté par
`#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)` — **un test de plateforme**. Le
brancher sur `fragmentTextureUnits` (posé à l'étape 1) le rendrait piloté par la
**limite mesurée**, ce qui est la règle. *Ce n'est plus un correctif Web : c'est
la suppression d'un `si (web)`.*

**Chiffrage : une demi-journée** pour (a) par la voie (2), banc à l'appui.

**Le détail — quatre fusions, aucune perte notable :**

| # | sacrifice | gain | coût visuel |
|---|---|---:|---|
| 1 | **`tShadowAtlasRaw` fusionné** avec `tShadowAtlas` (même texture, deux vues) | **1** | nul si le PCF passe par la vue *compare* |
| 2 | **IBL fusionnée** : `tEnvIrradiance` + `tEnvPrefilter` → un seul cube, irradiance au mip le plus haut | **1** | faible — c'est l'approximation classique |
| 3 | **`tLTC1`+`tLTC2` → un atlas 2D** (deux LUT 64×64, elles tiennent côte à côte) | **1** | nul |
| 4 | **`tSkyEnvCube` réutilise le cube IBL** quand le ciel est la source | **1** | nul dans la démo |
| | **total par FUSION** | **4** | 27 − 10 − 4 = **13 ≤ 16** ✅ |
| — | 🗄️ **RÉSERVE DISPONIBLE, non appliquée** : `tVoxelOpacity` et `tMatcap` éteints (repli : AO analytique, matcap neutre) | *2* | *visible. **Ce n'est pas un renoncement** : c'est deux unités qu'on sait où prendre, le jour où une cible descendrait sous le budget actuel. On ne les dépense pas parce qu'on n'en a pas besoin.* |

📌 **Marge : 3 unités, pas 0.** *Un correctif qui atteint pile la limite recasse
au prochain sampler ajouté* — c'est littéralement ce qui s'est produit le
11 août. Et désormais le **banc** (ci-dessous) le dira le jour même.

### 🟢 FEU VERT DE RODOLF (02/09), aux conditions posées

1. **le bureau garde ses 17 échantillonneurs** — la variante réduite ne concerne
   qu'une cible à 16 unités ;
2. **la sélection appartient au moteur** — aucun `#ifdef` dans le shader ;
3. **fusionner avant d'éteindre** — et la fusion suffit, donc rien ne s'éteint.

⚠️ **Deux garanties à prouver, pas seulement à annoncer :**
- **le bureau ne bouge pas d'un pixel.** Preuve : paire de captures Windows
  avant/après, plus le témoin de flux. *Un correctif « pour le Web » qui déplace
  un pixel sur le bureau est une régression déguisée en amélioration.*
- **le banc de budget existe AVANT la variante** — fait, voir ci-dessous.

**Chiffrage restant : ~1,5 à 2 jours** (étape 1 : une journée ; étape 2 : les
quatre fusions + une capture A/B chacune).

### 🧪 LE BANC — ce défaut doit rougir à la construction, pas dans ta console

**Spécification, non codée** *(même raison)* :

> Pour chaque étage fragment du dépôt, compter les échantillonneurs **après** les
> transformations de la cible (donc après `NkWebMergeCookieSamplers` pour le Web)
> et **échouer** si le compte dépasse le budget de la cible.

**Quatre exigences, et ce sont elles qui font la différence entre ce banc et un
banc qui compte pour rien :**
1. il compte **après transformation**, pas sur la source — sinon il mesure un
   objet que la cible ne verra jamais ;
2. il porte **le budget de chaque cible**, pas une constante — 16 pour
   WebGL2/GLES, davantage sur bureau ;
3. **contrôle positif obligatoire** : ajouter un sampler bidon à `PBR` doit le
   faire **rougir**. Sans cette contre-épreuve, un banc qui compte mal reste vert
   pour toujours — *un zéro n'est un résultat qu'après un contrôle positif* ;
4. il tourne **à la construction** : la limite est statiquement connue, elle n'a
   pas besoin d'un GPU pour être vérifiée.

### ✅ LE BANC EXISTE — `Tools/verif_budget_samplers.py`, écrit AVANT la variante

```
$ python Tools/verif_budget_samplers.py --cible web --controle
🔴 DEPASSEMENT  Resources/NKRenderer/Shaders/PBR/NkSL/pbr.frag.nksl
   17 echantillonneurs pour 16 accordes — 1 de trop.
   tAlbedo, tNormal, tORM, tEmissive, tHeight, tEnvIrradiance, tEnvPrefilter,
   tBRDFLUT, tSkyEnvCube, tVoxelOpacity, tShadowAtlas, tShadowAtlasRaw,
   tLight3DCookie0, tLight3DCubeCookie0, tMatcap, tLTC1, tLTC2
--- CONTROLE POSITIF ---
✅ (a) compteur sensible : les 185 etages gagnent exactement 1.
✅ (b) frontiere juste : 16 = vert, 17 = ROUGE.
                                                             → sortie 1
$ python Tools/verif_budget_samplers.py --cible desktop --controle
Aucun depassement — 185 etage(s) fragment mesures, budget 32.  → sortie 0
```

**Il retrouve les 17 noms un par un** — le même compte que la mesure manuelle,
obtenu autrement. Rouge sur Web, vert sur bureau : le budget par cible n'est pas
décoratif.

🔴 **ET MON PREMIER CONTRÔLE POSITIF ÉTAIT FAUX — il s'est dénoncé au premier
essai.** Il injectait un sampler dans chaque étage et comptait les
dépassements : le total ne bougeait pas, donc l'instrument se déclarait muet.
**Cause : la donnée d'essai ne pouvait pas exprimer l'écart.** `PBR` dépassait
déjà (17 → 18, toujours « 1 dépassement »), et l'étage suivant est à 9 —
injecter 1 ne le fait pas traverser 16. *Le dépôt ne contient aucun shader assis
sur la frontière.*

C'est la porte de la maison appliquée à moi-même : *quand une mutation survit,
la cause n'est pas toujours « le contrôle manque » — c'est souvent « le jeu
d'essai ne peut pas exprimer l'écart ». Le juge était bon, la question mal
posée.* Le contrôle éprouve maintenant **la frontière elle-même**, sur deux
étages synthétiques à `budget` et `budget+1` : un `>` écrit `>=` n'y survit pas.

📌 **Un défaut d'instrument attrapé au passage** : la console Windows est en
cp1252, et l'emoji de verdict faisait **tomber le script** par
`UnicodeEncodeError` — *un banc qui plante avant d'imprimer son verdict est
indiscernable d'un banc vert quand on lit son code de sortie à travers un
`| head`*. Flux reconfigurés en UTF-8. Cousin exact de l'`EXIT=0` déjà signalé
dans ce dossier.

*C'est le seul banc de ce lot qui aurait attrapé le défaut le 11 août à 00h01.*

---

---

---

## 11. 🦴 UNIFIER LES DEUX CONVENTIONS DE POSE DE REPOS ? — question posée, non tranchée

**Le contexte** : tu as approuvé la descente de `NkSkeletonDef` dans `NKAnima`
(bloc 6, point 1). En la préparant, j'ai mesuré les deux structures qui vont
cohabiter dans `nkentseu::anim` — et **elles ne se ressemblent pas**.

| | `anim::NkRetargetSkeleton` (noyau, existant) | `NkSkeletonDef` (celui qui descend) |
|---|---|---|
| forme | **4 tableaux parallèles** : `parent`, `bindLocal`, `names`, `topo` | **1 tableau de structures** : `bones[]` de `{name, parent, bindPose, inverseBindPose}` |
| pose de repos | `NkMat4f` **LOCAL**, relatif au parent | matrices **bind / inverse-bind** (monde) |
| ordre | `topo` explicite (parents avant enfants), avec détection de cycle | implicite : `parent < i` supposé |
| nom | `NkString` | `char[64]` |
| services | `BindWorldPos`, `BindWorld`, `BindHeight`, `BuildTopo` | `FindBone` |

> 🔴 **Ma conclusion, et elle est nette : ce ne sont pas deux versions d'une même
> chose, ce sont deux structures différentes.** L'une décrit un squelette
> **à recibler** (on part d'une pose locale, on dérive le monde par FK) ; l'autre
> décrit un squelette **à peaufiner pour le GPU** (les matrices inverse-bind sont
> exactement ce que la peau consomme). Chacune est dans la forme qui sert son
> usage.

**Le déplacement n'a donc PAS besoin de les unifier**, et je ne les unifie pas :
elles coexistent dans `nkentseu::anim`, comme deux types voisins et distincts.

### Ce que l'unification coûterait, et ce qu'elle apporterait

| | |
|---|---|
| ✅ **apport** | **un seul squelette dans le moteur.** Aujourd'hui, recibler une animation vers un personnage Noge demande une conversion à la main, que personne n'a écrite — donc le reciblage (660 lignes, livré le 06/08) **ne sert pas encore Noge**. Unifier, c'est brancher l'un sur l'autre. |
| ✅ **apport** | une seule convention à apprendre, un seul format `.nkskel` le jour où il existera. |
| 💸 **coût** | `BindWorld()` est une **FK récursive** : passer de local à monde est un calcul, pas une lecture. Le faire à chaque image serait une régression ; le faire à la construction demande un cache — donc un troisième état. |
| 💸 **coût** | `topo` + détection de cycle n'existent pas côté `NkSkeletonDef` : soit on les ajoute (et on alourdit l'actif partagé), soit on les perd (et on réintroduit le risque de boucle infinie que `BuildTopo` attrape). |
| 💸 **coût** | `NkString` contre `char[64]` : l'actif partagé est **copié par valeur dans l'ECS** au moment de la construction. Un `NkString` y met une allocation par os. |
| 🔴 **risque** | six consommateurs recompilent, et **c'est un changement de contrat, pas un déplacement** — le recensement doit passer par le compilateur, comme pour `NkSkeleton`, où `NkAssetIODemo` atteignait le type par un **champ** sans jamais écrire son nom. |

### 🔵 Ce que je recommande, et pourquoi c'est « pas maintenant »

**Garder les deux, et écrire UNE fonction de conversion** `NkRetargetSkeleton →
NkSkeletonDef`, le jour où un cas réel la demande — c'est-à-dire le jour où on
recible une animation sur un personnage Noge. Aujourd'hui **aucun code ne fait ce
trajet** : unifier maintenant, ce serait payer un changement de contrat pour un
besoin que rien n'exerce.

*C'est la même règle que pour `NKAnimPhysics` au bloc 7 : on l'exerce quand un
jeu jouera l'équilibre, pas avant.*

**Ta décision** : (a) on garde les deux et on écrit la conversion au premier
besoin réel — ma recommandation ; (b) on unifie maintenant, en acceptant le
changement de contrat et sa journée de recensement au compilateur.


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
- **HarmonyOS rend la 3D** — image prise et HUD lu le 02/09 à 19h31, sur le `.hap`
  du 10/08. **4ᵉ cible verte sur matériel réel.**
- **Le défaut Web est NOMMÉ et DATÉ** — `PBR` demande 17 unités de texture pour
  16 accordées ; franchi le **11/08 à 00h01** (tables LTC), invisible 22 jours
  parce que SwiftShader en accorde plus de 16. Doublure conçue et chiffrée
  (~2-3 j), banc spécifié. **Section 10.**
- **`--demo 2` et `demo 2` ne retombent plus en silence sur la démo 0** — et la
  vraie cause du symptôme était le **répertoire courant** (2 erreurs en démo 0,
  **47 en démo 3D dont 18 shaders introuvables** : les deux chemins n'ont pas la
  même sensibilité à la même ressource manquante).
