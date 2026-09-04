# Noge — ce qui attend une décision de toi

> Une page. Le détail est dans `Engine/Noge/echanges_noge.md` (rapport complet).
> Branche `feat/noge-inventaire`, arbre `Nkentseu-noge`.
> **Mise en ordre du 2026-09-02, en fin de journée.**

> # 🟢 RODOLF — LE WEB REND. Relance : c'est plus léger.
>
> **Le correctif PBR tient, tu l'as confirmé.** Le paquet a maigri depuis :
> **−247 976 octets (−24,7 %)**, **188 fichiers en moins**, et **~950 écritures
> console au chargement ramenées à ~21**.
>
> ```
> Build\Bin\Release-Web
enderdemo
enderdemo.bat 9002
> http://localhost:9002/renderdemo.html?demo=2
> ```
>
> **Deux choses à me renvoyer**, et elles tiennent en un copier-coller :
> 1. la ligne **`Demo 3D | API : …`** et le panneau **`Shadow tweak`** — c'est ce
>    qui fait passer la colonne Web au **vert** ;
> 2. la ligne neuve **`[NkWeb] preparation terminee en <N> ms pour 288
>    dependances`** — c'est elle qui dira si la lenteur est réglée, **en
>    chiffres**. Elle n'existait pas hier.
>
> 📌 **Et l'essai à cinq secondes, avant tout le reste : ferme les outils de
> développement et recharge.** Si ça change tout, la lenteur était la console —
> et le palier que je viens de poser l'a déjà traitée. Si ça ne change rien,
> c'est ailleurs, et la ligne de mesure ci-dessus dira où.
>
> ⚠️ **L'ombre est plus douce qu'avant, c'est attendu** — repli PCF 3×3, le PCSS
> coûtait l'unité de texture qui manquait. Ce n'est pas un défaut.

> ### 🗄️ Historique — la relance d'hier soir
>
> **Tes deux essais ne testaient pas le correctif** : tes wasm dataient de
> **09h13 et 10h41**, le correctif de **22h50**. Tu n'as jamais eu le bon binaire
> entre les mains. **Les deux arbres sont reconstruits (23h25 et 23h27, 30/30) et
> j'ai vérifié que le correctif est DANS les fichiers**, pas seulement qu'il
> compile.
>
> **Peu importe le port : les DEUX arbres sont corrects maintenant.**
>
> ```
> Build\Bin\Release-Web\renderdemo\renderdemo.bat 9002
> http://localhost:9002/renderdemo.html?demo=2
> ```
>
> ### 🔑 LE TÉMOIN — une ligne, dans la console, qui ne peut pas mentir
>
> ```
> [NkRHI_GL] budget d'unites de texture : 17 demandees pour 16 accordees
>            -> PCSS retire (repli PCF 3x3), 16 restantes
> ```
>
> **Si tu vois cette ligne, tu es sur le bon binaire et le correctif s'est
> déclenché.** Elle n'existe que depuis ce soir, et elle s'imprime sur le chemin
> NORMAL — pas sur un échec.
>
> ⚠️ **N'utilise PAS « absence de `[WebDiag]` » comme témoin, je me suis
> corrigé** : le message `link FAIL` porte lui aussi l'étiquette `[WebDiag]` et
> il n'est **pas** conditionné par le drapeau de diagnostic. Voir des `[WebDiag]`
> ne dirait donc pas « vieux binaire », ça pourrait aussi dire « nouveau binaire,
> et ça a encore échoué » — *un témoin qui confond deux causes ne tranche rien.*
>
> ### Ce que tu dois voir si le correctif tient
>
> - **plus de `link FAIL` sur `PBR`** ;
> - la ligne **`Demo 3D | API : OpenGL`** et le panneau **`Shadow tweak`** ;
> - **l'ombre est plus douce** — c'est le repli PCF 3×3, **c'est attendu, ce
>   n'est pas un défaut**. Le PCSS (durcissement au contact) est retiré sur cette
>   cible : il coûtait l'unité de texture qui manquait, et il était **déjà
>   désactivé** par le palier de qualité mobile.
>
> 📌 **La leçon, et elle est structurelle** : *un correctif compilé mais non
> déployé est indistinguable d'un correctif absent, du point de vue de celui qui
> teste.* On a passé la journée à séparer « ça compile » de « ça tourne » — voici
> le troisième état, entre les deux : **ça compile, ça ne tourne pas encore chez
> toi**. Le livrable d'un correctif de cible n'est pas le commit, **c'est le
> binaire que tu lances**.


## 📌 CE QUI RESTE, ET CE QUI NE T'ATTEND PLUS

**Cinq des huit blocs sont tranchés.** Ils restent écrits, avec leur réponse et
sa date — *une feuille qui garde des questions déjà répondues fait perdre du
temps ; une feuille qui efface l'historique fait re-trancher.*

### 🟠 CE QUI T'ATTEND ENCORE — **trois choses**, le point B est tombé ce soir

| # | ce qu'il faut | de qui | coût |
|---|---|---|---|
| **A″** 🟠 | ✅ **CORRIGÉ le 02/09 — il te reste 2 minutes.** Le défaut est traité : **17 → 16**, par un déclencheur qui **ne nomme aucune plateforme** (il compare la demande du shader au budget du pilote). Constructions vertes, banc vert. ⚠️ **Marge ZÉRO** — 16/16, l'état exact qui a explosé le 11/08 ; le banc est désormais le rempart, et la réserve IBL/sky rendra 2 unités quand le GPU sera libre. **Ce qu'il manque : une exécution sur ta carte**, port **9002**. Historique : ✅ **FAIT, et ça avait RÉFUTÉ le vert Web.** Tu as lancé sur ta carte : `PBR` ne se lie pas — **17 unités de texture demandées, 16 accordées**, écran vide. Le vert d'hier venait de SwiftShader, plus permissif que le matériel. **Ce qui t'attend maintenant, ce n'est plus un test, c'est une décision** : lancer la **variante réduite de `PBR`** (conçue, chiffrée **~2-3 j**, non codée). ⚠️ macOS/iOS étant bloquées par la signature, **ce défaut coûte 3 plateformes sur 7**. Tout en **section 10**. | **toi** — dire quand | ~2-3 j |
| ~~**B**~~ | ✅ **FAIT le 02/09 à 19h31 — HarmonyOS REND LA 3D.** Tu as lancé l'émulateur, j'ai installé le `.hap` du 10/08 et **lu le HUD moi-même** : `Demo 3D | API : OpenGL`, panneau `Shadow tweak`, `FPS approx : 8.3`, 17 sphères PBR + ombres portées. **5 cibles sur 7.** ⚠️ Réserve écrite : binaire du **10/08**, donc l'image prouve « HarmonyOS rendait la 3D le 10/08 » — un re-test sur un `.hap` à jour reste à faire, **comme pour Linux**. Détail : **carte, section 9**. | — | fait |
| **C** | **Re-tester Linux sous WSL.** Le vert repose sur la capture du 29/07 + ton témoignage ; le build d'aujourd'hui n'y a jamais tourné. WSL2 n'a pas répondu en 120 s pendant cette session. | toi (débloquer WSL), puis moi | ~10 min |
| **G** ✨ | **Les particules : finir le renderer, ou pas maintenant ?** Mesuré le 03/09 : la simulation tourne (401 vivantes), mais **aucun pipeline VFX n'a de shader**, **personne n'appelle `Update`**, et les quads ont une **aire nulle** — zéro pixel sur les deux backends. Trois pièces, ~1,5 j, ordre et témoins au **bloc 13**. | **toi** | dire quand |
| ~~**F**~~ 🚗 | ✅ **TRANCHÉ (a) ET LIVRÉ le 03/09.** Le couple s'intègre pour tout le monde (`d471956d`, bancs des consommateurs au même compte) ; `NkVehicle` livré selon la conception — roue par raycast, suspension à trois gardes, adhérence en vitesse à annuler bornée par le cercle de friction, **dans** le pas fixe, surface à 16 lignes. Banc **48/48** avec contre-épreuve (`µ = 0,01` → elle patine) et **deux mutations prouvées**. Reste : Ackermann, réglage sur les deux voitures du dépôt. Détail : `CONCEPTION_VEHICULE.md` §7. | — | fait |
| **F′** 🚗 | **Le jeu de voiture peut s'ouvrir** — c'était ta condition : *« si et seulement si la physique est prête »*. Elle l'est, headless. **Ce qui manque pour le dire à l'image** : une capture d'une voiture qui roule dans `renderdemo` (GPU → Ilyana d'abord). | **toi** | dire quand |
| **F₀** 🚗 | **La physique de véhicule : (a) ou (b) ?** *(historique)* La conception est écrite (`Engine/Noge/CONCEPTION_VEHICULE.md`, ~2,5-3 j). ⚠️ **Une seule question t'attend** : l'étape 0 corrige `NkIntegrator` — le champ `torque` existe et **n'est jamais intégré** — donc pour **tout le monde**, ragdoll compris. **(a)** on corrige le socle (ma recommandation : c'est un défaut, pas un choix) ; **(b)** le véhicule recopie chez lui. | **toi** | une phrase |
| ~~**E**~~ | ✅ **FAIT le 04/09.** `NkRetargetSkeleton` a disparu, `NkSkeletonDef` est LA structure, conversion à l'import (`FromLocalBind`), local dérivé. Témoin : conversion qui se retourne à 1e-4 sur un repos incliné + reciblage aux mêmes poses qu'avant. Détail bloc 11. | — | fait |
| **E₁** | *(historique)* 🟢 **TRANCHÉ (04/09) : on unifie dans `NKAnima`.** Rodolf, contre ma recommandation — et une mesure prise après sa décision lui donne raison : `NkRetargetSkeleton` **n'a aucun consommateur hors du module**, l'unification ne casse donc aucun contrat public. Plan écrit au **bloc 11**, ~½ j, **non exécuté, rien ne bloque**. | — | à faire |
| **E₀** | 🦴 **Faut-il UNIFIER les deux conventions de pose de repos ?** *(historique)* `NkRetargetSkeleton` (local relatif au parent) contre `NkSkeletonDef` (matrices bind/inverse-bind). Mesure faite : **ce ne sont pas deux versions d'une même chose**. Coûts, apports et recommandation au **bloc 11**. | **toi** | une phrase |
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
| 4 | **Web** | 🟠 | **CORRIGÉ, non revérifié.** Le défaut est nommé et traité (17 → 16, section 10) ; il manque **une exécution** sur ta carte pour le confirmer. Au soir du 02/09 il était ROUGE : `PBR` ne se liait pas — `texture image units count exceeds MAX_TEXTURE_IMAGE_UNITS(16)`, écran vide. 🟡 **Vert en LOGICIEL** : `Captures/plateforme_web_2026-09-02.png` reste vraie, prise sous SwiftShader qui accorde **plus de 16** unités | **2 minutes de navigateur** pour confirmer le correctif — ⚠️ sur le port **9002** (Release) |
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

### ✅ (a) LIVRÉ — 17 → 16, et **ce n'est pas un correctif Web, c'est la suppression d'un `si (web)`**

**Le code** : `NkTrimShadowRawSampler` + `NkCountDeclaredSamplers`, dans
`NkOpenglDevice.cpp`, **volontairement HORS de toute garde de plateforme** — et
sans aucun des helpers `NkWeb*`, qui vivent, eux, sous la garde. *Une fonction
pilotée par une capacité ne doit pas dépendre d'un `#if` de cible, sinon elle
redevient un `si (web)` par la porte de derrière.*

**Le déclencheur ne nomme personne** :

```cpp
if (glStage == GL_FRAGMENT_SHADER && mCaps.maxFragmentTextureUnits > 0 &&
    NkCountDeclaredSamplers(src) > mCaps.maxFragmentTextureUnits) { … }
```

> Il compare **ce que le shader demande** à **ce que le pilote accorde**. Pas de
> plateforme, pas de preset, pas de constante 17 codée en dur dans NKRHI —
> *NKRHI n'a pas à connaître ses shaders.* Il se déclenchera tout seul sur
> n'importe quelle cible étroite, WebGL2 aujourd'hui, un GL ES pauvre demain,
> **sans que personne ne l'ait nommée**.

**Le repli est une dégradation, pas un trou** : le test `mode == 4` est
neutralisé, le flot tombe sur le **PCF 3×3** de fin de fonction, qui n'utilise
que le sampler comparatif. L'ombre devient plus douce, elle ne disparaît pas.

**Et le bureau ne bouge pas d'un pixel — par construction, pas par comparaison.**
La transformation travaille sur une **copie** ; sur bureau la condition est
fausse (32 accordées pour 17 demandées), `src` n'est jamais touché. *Une
non-régression prouvée par la structure vaut mieux qu'une non-régression prouvée
par deux captures qu'il faut savoir comparer.*

**Mesure — le banc, avant et après :**

```
$ python Tools/verif_budget_samplers.py --cible web
🔴 pbr.frag.nksl : 17 pour 16 — 1 de trop.                      sortie 1

$ python Tools/verif_budget_samplers.py --cible web --apres-trim --controle
Retrait pilote par le budget applique : tShadowAtlasRaw retire
(PCSS -> repli PCF 3x3 ; branche deja morte a NK_MOBILE)
✅ (a) compteur sensible : les 185 etages gagnent exactement 1.
✅ (b) frontiere juste : 16 = vert, 17 = ROUGE.
Aucun depassement — 185 etages mesures, budget 16.              sortie 0
```

**Constructions** : NKRHI **16/16**, NKRenderer **24/24**, **Noge 41/41** — le
même compte que la ligne de base prise avant d'ouvrir le lot.

⚠️ **CE QUE CETTE PREUVE NE COUVRE PAS, ET JE LE BORNE.** Le mode `--apres-trim`
du banc **réimplémente la règle en Python** ; il ne teste pas le C++. C'est
délibéré — *le juge doit venir d'ailleurs que le jugé* — mais il faut le dire
net : **la règle et son arithmétique sont prouvées (17 → 16), le code C++ est
compilé et non exécuté.** Sa vérification tient en une ligne, le jour où le Web
tournera : le journal doit afficher
`[NkRHI_GL] budget d'unites de texture : 17 demandees pour 16 accordees ->
PCSS retire (repli PCF 3x3), 16 restantes`.

### 🔴 MARGE ZÉRO — à dire fort, parce que c'est l'état qui a explosé le 11 août

**16 sur 16. Il n'y a plus une seule unité libre.** C'est *exactement* la
configuration qui a produit ce défaut : un budget atteint pile, qu'un seul
sampler ajouté fait basculer — et qui l'a fait basculer le 11/08 à 00h01, sans
que personne ne le voie pendant 22 jours.

**Ce qui change, et c'est la seule chose qui change** : le banc existe
maintenant. Le prochain sampler ajouté à `PBR` **rougira le jour même**, pas
22 jours plus tard, et pas dans ta console.

🗄️ **La réserve qui rétablira la marge** : fusionner l'IBL (`tEnvIrradiance` +
`tEnvPrefilter`) et le cube de ciel (`tSkyEnvCube`) rend **2 unités** → 14/16.
Elles demandent une capture A/B chacune, donc le GPU. **Ce n'est pas un
renoncement : c'est deux unités dont on sait où elles sont**, qu'on ne dépense
pas faute de besoin. *La cible tient sur un fil — le fil est solide, et il est
désormais surveillé.*

### 🔎 LES AUTRES `si (plateforme)` — recensés, nommés, **non corrigés**

Relevé sur NKRHI et NKRenderer. ⚠️ **Toutes les gardes de plateforme ne sont pas
de la même famille**, et les confondre ferait un mauvais lot :

| famille | exemples | verdict |
|---|---|---|
| ✅ **vraies différences d'OS** — une API existe ou n'existe pas | `_putenv_s` contre `setenv` (`NkGpuPolicy.cpp:33,43`), `NvOptimusEnablement` (`:8`), les 49 appels EGL gardés | **légitimes** — ce n'est pas une capacité, c'est un autre système |
| 🔴 **capacités déguisées en plateforme** — la vraie question est « de quoi la cible est-elle capable ? » | le **remap d'unités de texture** (`NkOpenglDevice.cpp:1965-1975` et `:2931`) : la table est calculée pour **`MAX_TEXTURE_IMAGE_UNITS=16` codé en dur**, sous `#if EMSCRIPTEN`, avec le commentaire *« SwiftShader n'accorde que 16 »* — **le nombre est écrit à la main là où `mCaps.maxFragmentTextureUnits` le dit maintenant** | **à basculer** — même geste que celui de ce lot |
| 🔴 même famille | **`NkWebMergeCookieSamplers`** lui-même (`:2013`), sous `#if EMSCRIPTEN`, alors qu'il répond à une contrainte d'unités | **à basculer** |

📌 **Le motif commun des deux 🔴** : *un 16 écrit à la main dans un commentaire,
sous une garde de plateforme.* C'est la forme exacte du défaut que ce lot vient
de corriger — **et c'est la forme exacte du « 24 → 14 » qui a vieilli en
silence.** Un lot propre les basculerait tous les trois sur
`mCaps.maxFragmentTextureUnits`, et supprimerait les trois derniers nombres
codés en dur. **Non fait ici : hors mandat, et ça mérite son propre lot.**

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

### 🗂️ 04/09 — NKAnima a une arborescence, et NKAnimPhysics y est entré

`src/NKAnima/` était **plat** (9 fichiers) ; y verser six paires de plus en aurait fait quinze en
vrac. Convention des voisins mesurée (anglais, PascalCase) → `Skeleton/ Clip/ Retarget/ Motion/
Physics/ Edit/`, un `NKAnima.h` d'agrégation à la racine et **rien d'autre** — *la racine est un
contrat, pas une salle d'attente*. `NkAnimationEditor` va dans `Edit/` parce que c'est un **modèle
sans UI** (lu : aucun include NKGui). `git mv` partout, l'histoire suit ; `NkAnimPhysTest` devient
`NkAnimaTest`, le banc du module ; `NKAnimPhysics.jenga` supprimée, **huit références** de build
retirées ou reportées ; **21 modules Runtime au lieu de 22**. NKRenderer dépendait déjà de NKAnima :
aucune dépendance nouvelle.

### 🔴 LE COMPTE ÉTAIT DE TROIS, PAS DEUX — et NKAnimPhysics rentre dans NKAnima (04/09, après-midi)

Rodolf : *« j'espère que ce n'est pas un chantier dupliqué »* puis *« pourquoi ne pas
mettre NkAnimPhysique dans NkAnima ? »*. Mesuré :

| structure | où | ce qu'elle redit | sort |
|---|---|---|---|
| `anim::NkSkeletonDef` | NKAnima | — | **la cible** |
| `anim::NkRetargetSkeleton` | reciblage | parent, repos local, noms, topo | ✅ **supprimée** (ci-dessous) |
| **`physics::NkBoneDef`** | `NKPhysics/NkRagdoll.h` | **`parent`** + corps rigide + joint | 🔴 **à absorber** : un os du squelette unifié + une table d'**attributs physiques** (forme, matériau, joint, limites) — jamais un second squelette qui redit `parent` |

**Deux prémisses corrigées par la lecture** : `NkClipBalancePass.h` ne fait que *citer*
`NkRagdoll.h` dans un commentaire — **aucune inclusion**, sa `.jenga` dit vrai ; et
`NkRagdoll::Build` **est implémenté** (inline) **et exercé** par `NKPhysics/tests/test_physics.cpp`
(deux sites) — pas « déclaré, non livré ».

**Le cycle qui décide du déménagement** : NKAnima dépend d'animphys ; dès qu'animphys consomme
`NkSkeletonDef`, animphys → NKAnima → animphys. On le rentre : `NKAnima/src/NKAnima/Physique/`,
mêmes noms de fichiers (`git mv`), espace `anim`, `.jenga` supprimée, **huit références** de build
retirées ou reportées, `NkAnimPhysTest` devient le banc de NKAnima. NKRenderer dépendait **déjà** de
NKAnima (mesuré) : aucune dépendance nouvelle. **Déclaré vs consommé** : NKRenderer inclut vraiment
(2 fichiers) ; RendererSandbox et Tutoriels3D déclarent sans inclure ; NkAnimaEditor inclut **sans
déclarer**.

**La conversion pose → positions monde vit en UN endroit** (NKAnima), pas une par appelant : les FK
existantes recensées — reciblage (`WorldOf` de test), `NkIKSolver`, `NkLocomotion`, `AnimBridge`
(éditeur) — sont autant de copies candidates à converger. Non fait dans ce lot au-delà du reciblage.

**Critères de fin, mesurables** : 21 modules Runtime au lieu de 22 · une seule définition de la
**topologie** (parent + repos) — `NkSkeletonDef` — les autres structs (pose par instance, IK,
jiggle) référencent par index et **aucune ne redit `parent`** (mesuré : 0 sur 4) · bancs
d'animphys à l'identique depuis leur nouvelle maison · consommateurs recompilés, la liste.

### 📍 04/09 — OÙ EN EST LE COMPTE, au grep, en fin de journée

`grep "int32 parent ="` dans NKAnima/NKPhysics/Noge/éditeur : **une** définition de type
(`NkSkeletonDef`). Reste une **copie de données** : `NkAnimationClip::jointParent/jointTopo` — le
clip embarque la topologie du squelette qu'il anime (le clip glTF la reçoit à l'import ; l'éditeur
et `ApplyFKSkinning` la lisent). Ce n'est pas une seconde *structure*, c'est un second *exemplaire*,
et un exemplaire peut diverger. La retirer, c'est faire passer le squelette à chaque consommateur
du clip (NKRenderer, Noge, éditeur) — un lot à part, à trancher, pas à glisser dans celui-ci.

### 📏 04/09 — LA COPIE `jointParent/jointTopo` DU CLIP : mesurée, pas tranchée

Lecteurs : `Clip/NkAnimation.cpp` (18 : **sérialisation** du clip — parents et topo font partie du
format de fichier —, FK, échantillonnage), `AnimBridge.cpp` (13), `NkGLTFAnimBake.cpp` (6, l'écrit
à l'import), `NkAnimRetarget.cpp` (2, l'écrit au reciblage), `NkRagdollBridge.h` (2). Retirer la
copie = changer le **format d'actif** et cinq consommateurs ; un clip qui *référence* son squelette
suppose que le squelette existe à côté du clip partout où il est chargé (le renderer charge des
clips sans NkSkeletonDef aujourd'hui). Avis inchangé — un clip référence, il ne recopie pas — mais
c'est un lot de format, à trancher avec Rodolf, pas à glisser en fin de journée.

### 🔎 04/09 — LES TESTS « FANTÔMES » : correction, et le tableau

Mon aveu du midi était mal formulé : `jenga test` **trouve** les 60 projets `*_Tests` — j'avais passé
le nom du module au lieu de `NKPhysics_Tests`. La cause réelle : **`Nkentseu.jenga:451-453`,
`dutc(enable=True)` / `dute(enable=True)`** (compilation *et* exécution des tests désactivées), depuis
`5d90c862` du 2026-03-12. Mesure : 77 fichiers, **64 compilent** en isolation, 13 cassés (9 modules),
20 modules déclarent `with test()` sans dossier ; `test_physics.cpp` lié à la main → **61/61**.
Tableau complet dans `echanges/noge.questions.md`. **Rien n'est branché** avant que Rodolf tranche.

### ✅ 04/09 — UN SEUL CONSTRUCTEUR DE RAGDOLL

**Mesuré avant de bouger.** Trois noms, pas trois constructeurs : (1) `physics::NkRagdoll::Build`
(vue + attributs explicites) ; (2) `NkRagdollBridge` (éditeur) — fabriquait **ses** corps depuis
`bindGlobal[]` et savait deux choses que `Build` ignorait : *dériver* les formes (capsule
joint→parent) et *ancrer* la racine (KINEMATIC) ; (3) `NkRagdoll` de Noge — un **composant ECS**
(liens os→entité + machine d'état), pas un constructeur : personne dans Noge ne remplit ces liens
(seul le banc, à la main, 1 os). Le survivant : `Build`. La dérivation (`NkRagdollAttrsFromSkeleton`)
vit à côté de lui dans NKPhysics ; le pont de l'éditeur n'est plus qu'un pont (vue depuis
`bindGlobal`, attributs dérivés, `ReadPose` pour le retour) ; le composant de Noge s'appelle
`NkRagdollComponent` — l'homonyme est à **0** au grep.

**Témoin, depuis les deux appelants** (NkSystemsRevivalTest, **55/55**) : attributs explicites →
chaîne pendue 0,000 m / coupée 4,987 m ; attributs dérivés → chaîne racine ancrée 0,000 m, **même
chaîne racine libérée 4,987 m** (rien d'autre ne retenait), coupée = trois racines ancrées, 0 joint,
personne ne tombe. Une première contre-épreuve « coupée tombe » était fausse *par construction* :
une racine dérivée est ancrée — le témoin devait libérer la racine, pas couper la chaîne.

### ✨ 04/09 — PARTICULES, borne 1 : le MÉLANGE déclaré est celui qui rend (la texture, pas encore)

`NkEmitterDesc::blend` était **déclaré et jamais lu** : un seul pipeline, Additive, pour tous — le
même défaut que les huit fois précédentes. Désormais **trois pipelines**, un par famille que
`NkBlendDesc` sait fabriquer (Additive, Alpha, Opaque), choisis à l'appel par le mélange de
l'émetteur. Les trois autres modes (`MULTIPLY`, `PREMULT`, `SCREEN`) n'ont **pas de fabrique** :
repli Alpha **dit une fois** sur stderr (`[NkVFX] NkBlendMode 3 non honore…`), jamais en silence.

**Témoin, en pixels** (`renderdemo --demo=2`, OpenGL, frame 170, capture headless `NK_CAPTURE`) :
même émetteur, `NK_VFX_BLEND=additive` → **2 426** pixels quasi blancs (le cœur s'empile jusqu'à
saturer) ; `alpha` → **1 027** ; `alpha` une seconde fois → 1 026 ; `multiply` (repli) → 1 021 —
c'est le fond seul. Le bruit run à run (émetteur stochastique) vaut 33 732 pixels différents ;
le sujet en fait 69 282. Image : `Captures/noge_particules_melange_2026-09-04.png` (additive |
alpha, côte à côte).

**Pas fait, dit clairement** : `NkEmitterDesc::texture` reste non lu — l'honorer demande le
chemin des descripteurs (jeu global Vulkan pour les pipelines VFX) et un sampler dans
`particles.frag.nksl` ; c'est la borne 2, un lot à part avec son propre témoin (une texture
asymétrique, pour que l'image dise si elle est lue et dans quel sens).

### ✅ 04/09 — UNE SEULE CONVERSION POSE → MONDE, dans `Skeleton/`

**Sept** boucles `world[j] = world[parent] × local[j]` vivaient dans sept endroits : le clip
(`ApplyFKSkinning`), le reciblage (`WorldOf`), `FromLocalBind` lui-même, l'IK de Noge
(`BuildWorldPose`, qui supposait *parent avant enfant* sans `topo`), et l'éditeur (trois
propagations partielles : chaîne IK fixée, joint saisi). Une seule désormais :
`NkForwardKinematics` / `NkSkeletonDef::LocalToWorld` (`Skeleton/NkSkeletonDef.h`), avec les
deux boutons dont l'éditeur a besoin (`skip[]`, `rootsFixed`) et le repli *ordre d'index* quand
`topo` est vide. **Mesuré en passant** : `BuildTopo` n'était appelé que par le banc — les
squelettes réels (glTF, démos) n'avaient **jamais** d'ordre topologique ; `NkGLTFIO` le calcule
maintenant **à l'import**. Témoin : test 0 juge la conversion contre la **géométrie à la main**
(joint 2 de la chaîne à 30° = (−0,5 ; 1,866 ; 0)), pas contre elle-même ; mutation du noyau
(`world[j] = local[j]`) → `[ FAIL ] M2` ; restauré → 13/14. Consommateurs reconstruits :
NkAnimaTest 26/26, Nogee 44/44, NkAnimaEditor 31/31, NkLocomotionDemo 41/41 (banc 9/0).

Désordre listé, pas corrigé : `NkAnimaEditor/NkRagdollBridge.h` construit *ses* corps rigides
depuis `bindGlobal` — une seconde fabrique de ragdoll à côté de `physics::NkRagdoll::Build`
(pas une seconde structure de squelette : il lit des matrices, pas des `parent`).

### ✅ 04/09 — LA TROISIÈME STRUCTURE A DISPARU : `physics::NkBoneDef` → vue du squelette + attributs

`NkRagdoll.h` portait un `NkBoneDef` (parent + corps + joint) : un second squelette qui **redisait
`parent`**. Remplacé par `NkSkeletonView` (parents + repos monde, tableaux bruts — NKPhysics
n'inclut pas NKAnima, la vue n'est pas une copie) et `NkRagdollBoneAttr` (type de corps, offset
du centre de masse, forme, matériau, joint et limites — *rien de topologique*). `Build(world,
vue, attrs)`. **Critère de fin au grep** : parmi `struct Nk*Bone*/Nk*Skel*`, seul `NkSkeletonDef`
définit `parent` ; `NkIKBone`, `NkJiggleBone`, `NkRagdollBoneLink` référencent par index.

**Deux choses apprises en le prouvant.** (1) `jenga test` répond *« No test projects found »*
même avec `--force`, alors que 42 `.jenga` déclarent `with test()` : **`NKPhysics/tests/
test_physics.cpp` n'est compilé par personne** — ma phrase de l'après-midi « exercé par
test_physics.cpp » était vraie du texte, fausse du binaire. Les deux sites y sont réécrits quand
même ; le **seul banc qui exerce `NkRagdoll::Build` est désormais NkSystemsRevivalTest**
(+4 checks, 52/52). (2) Épingler la racine en changeant `type = STATIC` *après* `Build` laissait
`invMass = 1` : le solveur poussait un mur et la chaîne s'affaissait de 1,26 m. Le type de corps
est un **attribut physique de l'os** (`NkRagdollBoneAttr::type`), `CreateBody` en déduit la masse.
Témoin : même table d'attributs, topologie **chaîne** → le dernier corps reste pendu (0,000 m) ;
topologie **coupée** → il tombe de 4,987 m en 1 s. Le témoin varie comme le sujet.

Désordre listé, pas corrigé : Noge a *son* `NkRagdoll` (`Physics/NkPhysicsMesh.h`, données du
composant) homonyme de `physics::NkRagdoll` — ambigu sous `using namespace`, à qualifier.

### 🧪 04/09 — LA PREUVE PAR MUTATION, et ce qu'elle a d'abord raté

Conversion à l'import cassée (`bindPose = loc`, FK sans le parent) → le banc doit rougir.
**Deux rounds l'ont laissée passer.** Non parce que le témoin était mauvais : parce que le
binaire ne contenait pas la mutation — un en-tête modifié, un build « SUCCESS », et une lib
ou un exe pas relié. *Un build vert qui ne reconstruit pas ce qu'on mesure est un build qui
ment* — troisième fois ce chantier. Nettoyage dur (objets + lib + exe supprimés), rebuild :
**abandon par assertion d'indice** avant même le rapport. Un témoin qui *plante* n'est pas
un témoin qui *rougit* : court-circuit posé après le test 0 (un squelette dont la conversion
est fausse ne nourrit pas les tests suivants). Résultat : mutation → **`[ FAIL ] M2`, 12/14** ;
restauration → **`[ OK ] M2`, 13/14**. Le témoin mord, proprement.

### ✅ EXÉCUTÉ LE 04/09 — une seule structure, la conversion à l'import, le témoin qui mord

**`NkRetargetSkeleton` n'existe plus** — pas un alias, pas un « au cas où » :
`grep` sur tout le code rend zéro, seules les archives (ce bloc, le rapport, la
roadmap du module) la nomment encore, datée. `NkSkeletonDef` a gagné exactement
ce que le plan disait : `topo` + `BuildTopo()` (détection de cycle conservée),
`BindLocal(j)` **dérivé** (`inverse(monde(parent)) × monde(j)`, jamais stocké),
`BindWorld/BindWorldPos/BindHeight`, `ParentVector()`, et **`FromLocalBind()` —
la conversion, à l'import, une fois** : FK dans l'ordre topologique, monde et
inverse-monde remplis, aucun actif produit à moitié si la hiérarchie a un cycle.

**Le reciblage consomme l'actif** : cinq signatures passent de
`const NkRetargetSkeleton &` à `const NkSkeletonDef &` ; `RetargetClip` prend
désormais `jointInverseBind` **dans l'actif** (`inverseBindPose`) au lieu de le
recalculer — une seule source de vérité pour la peau et le reciblage.

**Le témoin, et il est double :**
- **test 0, neuf** : sur une chaîne dont le repos est **incliné de 30°** (le cas
  où la convention diffère), chaque `BindLocal(j)` dérivé **redonne le local
  d'origine** à 1e-4, `monde × inverse-monde = identité`, et le monde **n'est pas**
  le local (sinon la conversion n'aurait rien fait) ;
- **test 3, inchangé dans ses attentes** : source au repos plat, cible au repos à
  30° → le bout de la cible reste **pile à sa position de repos**, et pas à
  celle de la source. *Mêmes poses à ε qu'avant l'unification* — c'est
  l'animation reciblée avant/après que la consigne demandait.

`NkAnimPhysTest` : `[ OK ] M2 NkAnimRetarget`, **13/14 suites** — le même compte
qu'avant (la 14ᵉ est `XBot.glb`, absent du dépôt, préexistant).

**Ce qui a recompilé — la liste, pas l'impression** : `NKAnima` 9/9,
`NkAnimPhysTest` 27/27, puis la chaîne des consommateurs de l'actif — voir le
message du commit pour les comptes.

### 🟢 TRANCHÉ PAR RODOLF (04/09) — **on unifie, contre ma recommandation**

> *« non, on les unifie dans NkAnima car elle sera utile pour plusieurs systèmes
> qui en auront besoin. »*

**Et une mesure prise après sa décision la rend beaucoup moins chère que ce que
j'avais chiffré.** Je m'étais arrêté sur « changement de contrat, six
consommateurs » — j'avais compté les consommateurs du **squelette**, pas ceux de
la **structure qui disparaît** :

| structure | qui la nomme, hors de NKAnima |
|---|---|
| `NkSkeletonDef` (celle qui reste) | `NkLocomotionDemo`, `NkSystemsRevivalTest`, `Noge/NkAnimation.h`, `Noge/NkGLTFIO.cpp` |
| **`NkRetargetSkeleton`** (celle qui disparaît) | **PERSONNE.** Elle ne vit que dans `NkAnimRetarget.{h,cpp}` — un seul module, un seul fichier de corps |

> 🔑 **Le seul appelant externe du reciblage est `NkAnimPhysTest`, et il passe
> par `NkAnimRetarget`, pas par la structure.** L'unification ne casse donc
> aucun contrat public : elle change les **paramètres de cinq fonctions
> statiques** d'un module dont un seul test se sert.

### Le plan, dans l'ordre — ce qui disparaît, qui consomme quoi, où vit la conversion

**1. Ce qui disparaît** : `struct NkRetargetSkeleton` (4 tableaux parallèles :
`parent`, `bindLocal`, `names`, `topo`). Ses quatre services **ne disparaissent
pas** — ils deviennent des fonctions libres sur `NkSkeletonDef` :
`BindWorld`, `BindWorldPos`, `BindHeight`, `BuildTopo`.

**2. Ce que `NkSkeletonDef` doit gagner** — et c'est le cœur de la décision :

| besoin du reciblage | dans `NkSkeletonDef` aujourd'hui | à faire |
|---|---|---|
| pose de repos **locale** | absente : il porte `bindPose` / `inverseBindPose` (monde) | **rien à ajouter** : le local se dérive — `local(j) = inverse(bindPose(parent)) × bindPose(j)` |
| `topo` (parents avant enfants) | implicite (`parent < i` supposé) | **champ ajouté**, construit à l'import, **avec détection de cycle** — c'est le seul filet que `BuildTopo` apportait, on ne le perd pas |
| `names` | `char[64]` dans `NkBoneDef` | rien — et on **gagne** : plus d'allocation par os, l'actif reste copiable par valeur |

**3. Où vit la conversion : À L'IMPORT, UNE FOIS.** C'est le point de la
consigne, et il est structurant — *une convention s'absorbe une fois, au moment
où la donnée entre, jamais dans une seconde structure qui la porterait en
parallèle.* Les importateurs (`NkGLTFIO`, FBX) écrivent déjà `bindPose` et
`inverseBindPose` ; ils ajouteront `topo` au même endroit. **Aucune conversion à
l'exécution**, donc pas de FK récursive par image — le coût que je redoutais au
tableau ci-dessous **disparaît avec la structure**.

**4. Ce qui recompile** : `NkAnimRetarget.{h,cpp}` (les cinq signatures et leur
corps), `NkAnimPhysTest` (le seul appelant externe), et **rien d'autre** — les
quatre consommateurs de `NkSkeletonDef` ne voient qu'un **champ ajouté**.
⚠️ Recensement **au compilateur**, comme toujours : `NkAssetIODemo` atteignait
`NkSkeleton` par un **champ** sans jamais écrire son nom.

**5. La preuve** : les six bancs au même compte (Noge 41/41, Nogee 45/45,
LocomotionDemo 9/0, AssetIODemo 55/0, SystemsRevivalTest 48/0, NkAnimPhysTest
13/14) **et** une contre-épreuve sur la conversion — un squelette dont le local
dérivé doit redonner le monde d'origine à ε près, sinon la conversion est
fausse et silencieuse.

**Chiffrage révisé : ~½ journée** (contre « une journée de recensement » quand je
croyais le contrat public). 🚫 **Non exécuté** : ce lot-ci livrait l'image de la
voiture, les particules et `?diag=1`. **Rien ne bloque** — c'est le prochain.

### 🗄️ Ce que j'avais recommandé, et pourquoi Rodolf a eu raison de trancher autrement

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


---

---

## 12. ⏱️ LA LENTEUR DE CHARGEMENT WEB — mesurée, deux causes traitées, une troisième nommée

> 🗣️ **Rodolf, 2026-09-03** : *« ça prend, mais c'est hyper lent. »*
> ✅ **Le correctif PBR tient** — le Web rend. Ce bloc traite ce qui reste.

### 📏 CE QUI ÉTAIT EMBARQUÉ, ET CE QUE LE MOTEUR OUVRE VRAIMENT

Le paquet web embarquait **l'arbre `Resources/NKRenderer/Shaders` en entier** :
**474 fichiers, 1 005 867 octets**. Mesure de ce que le chargeur construit
comme chemins — `NkShaderLibrary.cpp:679` et `:723` :

| | | |
|---|---|---|
| `<Mat>/NkSL/<mat>.{vert,frag}.nksl` | **préféré** | 103 fichiers |
| `<Mat>/VK/<mat>.{vert,frag}.vk.glsl` | **repli** — *« tous les backends chargent le `.vk.glsl` »*, DX11/DX12/Metal convertissant **à chaud** | 94 fichiers |
| `Include/*.glsli` | résolus par l'`IncludeResolver` | 8 fichiers |

🔴 **Les `.hlsl` et `.msl` ne sont lus par PERSONNE depuis cet arbre.** Ce ne
sont pas « des dialectes inutiles sur le web » : ce sont des **sorties** du
convertisseur (`NkShaderLibrary` `reportAndSave` VK→HLSL/MSL), versionnées à
côté de leurs sources. Les seules lectures de `.hlsl` du dépôt visent
`Resources/Shaders/Model/` — **un autre dossier**, pour `Applications/Model`.

### ✅ CE QUI EST FAIT — et le chiffre, pas l'impression

| | avant | après | gain |
|---|---:|---:|---:|
| `renderdemo.data` | 1 005 867 o | **757 891 o** | **−247 976 o (−24,7 %)** |
| entrées du paquet | 476 | **288** | **−188** |
| `.hlsl` / `.msl` | 126 / 62 | **0 / 0** | — |
| `.nksl` / `.vk.glsl` / `.glsli` | 103 / 94 / 8 | **103 / 94 / 8** | intacts |
| écritures console au chargement | **~952** | **~21** | **−98 %** |

**Piste 1 — le filtre** : `--exclude-file *.hlsl` et `*.msl` sur le
`--preload-file`. Les deux arbres sont reconstruits (Release **et** Debug,
30/30, `.data` identiques à 757 891 o).

**Piste 2 — la journalisation** : `monitorRunDependencies` appelait
`Module.setStatus` à **chaque** dépendance, et `setStatus` faisait un
`console.log`. Soit ~952 écritures. **Avec les outils de développement ouverts —
et Rodolf les a — une écriture console coûte des centaines de fois un `printf`** :
chaque ligne est formatée, horodatée, rattachée à une pile et rendue dans le DOM.
*C'est le seul coût de chargement qui GROSSIT quand on l'observe.* Palier à 5 %.
⚠️ **La barre et le texte à l'écran restent mis à jour à chaque fichier** : ils ne
coûtent rien, et c'est ce que l'utilisateur regarde. **On bride la console, pas
l'interface.**

⏱️ **Et le paquet mesure désormais son propre temps** — deux lignes neuves :
```
[NkWeb] preparation : 288 dependances
[NkWeb] preparation terminee en <N> ms pour 288 dependances
```
*Sans compteur, la prochaine comparaison serait un ressenti.* La ligne d'après
donnera **combien**.

### 🔎 CE QUI N'EST PAS FAIT, ET POURQUOI

**Les 68 `.gl.glsl` (105 405 o) restent embarqués.** Le code mesuré ne construit
**aucun** chemin `GL/` — mais **deux commentaires du dépôt affirment le
contraire** (`NkRender3D.cpp:496`, `NkPostProcessStack.cpp:346`). Tant qu'une
exécution n'a pas tranché entre le code et ses commentaires, **on ne retire
pas** : le gain certain d'abord, l'incertain après. *Retirer 105 Ko sur la foi
d'une lecture, contre deux commentaires qui disent l'inverse, c'est exactement
la conception sur inventaire qui s'est trompée hier.*

**Piste 3 — le nombre d'entrées MEMFS** (288 créées une par une) : non ouverte.
Un paquet unique ou un index éviterait le coût par entrée, mais **on mesure
d'abord** ce que les deux premières pistes ont donné. Il se peut qu'il ne reste
rien à gagner.


---

---

## 13. ✨ LES PARTICULES — mesurées avant d'être ouvertes : **il y a du vrai code**

> Consigne : *cherche où vit le corps, ne compte pas un suffixe* — souvenir de
> `NkCGXDetect` (1 347 l. d'en-tête, un `.cpp` de trois lignes) et de `NKGraph`
> (zéro `.cpp`, 1 519 l. dans les `.inl`).

### Ce que la mesure donne

| | mesure | verdict |
|---|---|---|
| `NKRenderer/Tools/VFX/NkVFXSystem.cpp` | **460 lignes**, **0 corps vide** | 🟢 **du vrai code**, pas une coquille |
| `NkVFXSystem.h` | 210 l., 5 inline | déclarations + petits accesseurs |
| ce qu'il expose | **émetteurs** (`CreateEmitter`, `Burst`, `SetEmitterPos`), **traînées** (`AddTrailPoint`), **décalques** | trois familles, pas une |
| `NkEmitterDesc` | ~20 champs réglés : formes d'émission, débit, rafale, durée de vie, vitesses, tailles, **dégradé de couleur**, gravité, dispersion | une vraie surface d'auteur |
| **appelants réels** | `NkRendererImpl`, `Noge/ECS/Systems/NkParticleSystem`, `NK3DModeler`, `DemoRW`, `Sandbox`, `NkSimulationRenderer` | 🟢 **exercé**, pas déclaré-inerte |
| pont ECS de Noge | `NkParticleSystem::Execute` — **50 l. avec un vrai corps** : `Query<NkParticleEmitter, NkTransform>`, recréation sur `dirty`, synchro de position | 🟢 |

> 🔑 **Conclusion : le chantier « particules » n'est PAS « écrire des
> particules ».** Le socle existe, il est branché, et un jeu peut déjà émettre.
> Ouvrir ce chantier en croyant partir de zéro aurait produit un doublon — *le
> peintre écrit deux fois*.

### Deux bonnes nouvelles mesurées, qu'on aurait pu croire fausses

1. **Aucun `compute`, aucun SSBO** dans le VFX (0 occurrence). La simulation est
   **CPU**, les particules sont montées en **billboards** dans un VBO
   (`ParticlesBillboard`, « 4 verts billboard »). ✅ **Donc le VFX tourne sur
   WebGL2** — contrairement aux trois doublures cloth/hair/softbody du bloc 2,
   qui sont en compute et restent absentes du Web. *La cible étroite ne coûte
   rien ici.*
2. **Aucun geometry shader utilisé.** Des `particles.geom.*` existent bien dans
   `Resources`, et `NkShaderLibrary` sait charger un étage géométrie — mais
   `NkVFXSystem` n'en demande aucun. ✅ **Second obstacle WebGL2 évité** (GLES
   n'a pas d'étage géométrie). ⚠️ Ces `.geom` rejoignent donc les `.hlsl`/`.msl`
   du bloc 12 : **des artefacts versionnés que personne n'ouvre**.

### 🔴 RÉPONSE MESURÉE LE 03/09 — « est-ce que ça REND ? » : **NON.** Et ce n'est pas le dessin qui manque, c'est tout ce qui est autour.

**Ce qui a été fait pour le savoir** — une sonde dans `Demo3D`, sous
`NK_VFX_PROBE=1` seulement (zéro effet sinon), en trois temps, chaque temps
tranchant une hypothèse :

| temps | geste | mesure | ce que ça tranche |
|---|---|---|---|
| 1 | créer **un émetteur** (400/s, 1 000 max) et capturer la frame 90, backend **logiciel** puis **OpenGL** | `emetteur cree id=1`, passe `VFX` exécutée 3×/frame, **0 erreur**, **0 pixel** sur les deux images | le système est branché ; rien ne s'affiche |
| 2 | faire **avancer** la simulation (`vfx->Update(dt, cam)`) | **toujours 0 pixel** | ce n'était pas *que* le tick |
| 3 | **compter** | vivantes : **136 → 230 → 318 → 401** aux frames 30/60/90/120 (≈ 400/s × 0,25 s = 100 par tranche : cohérent) | **simulé, pas dessiné** |

**Puis la lecture, guidée par les chiffres — trois manques, indépendants :**

1. 🔴 **Personne ne fait avancer la simulation.** `NkVFXSystem::Update(dt, cam)` n'a **aucun appelant actif** dans le dépôt : ni `NkRendererImpl` (qui crée le VFX à `InitVFX` et le *dessine* dans la passe `VFX`), ni le pont Noge (`NkParticleSystem.cpp:14` : *« l'animation des particules est faite par le pipeline NKRenderer »* — **faux**), ni aucune démo active. Le seul appelant est `Demo06_10.cpp.legacy`, retiré le 08/05. *Une passe qui dessine une simulation que personne n'avance.*
2. 🔴 **Aucun des trois pipelines VFX n'a de shader.** `ParticlesBillboard`, `TrailMesh`, `Decal` : rasterizer, profondeur, mélange, un `debugName` — **ni `shader`, ni `vertexLayout`, ni `topology`**. Et `NkOpenGLDevice::CreateGraphicsPipeline` **rend `{}` quand `d.shader` est introuvable** (`NkOpenglDevice.cpp:2370`). `BindGraphicsPipeline(invalide)` ne lie rien, `Draw(vivantes × 4)` part sans programme. **Le dessin VFX est un échafaudage** — et ça ne s'est jamais vu parce que rien n'avançait la simulation (manque 1) : `aliveCount == 0` court-circuitait le dessin avant qu'il ne puisse échouer.
3. 🟠 **Même avec un shader, les quads ont une aire nulle.** Le CPU écrit **quatre sommets à la même position** (`v.pos = p.pos`, seul `uv` change), la taille voyage en attribut ; le commentaire dit *« expansés dans le vertex shader ou ici »* — **ni l'un ni l'autre**. Les shaders GL historiques (`Particles/GL/particle.vert`) sont un *pass-through* prévu pour un **geometry shader** (`particles.geom.*`) que le VFX ne lie pas ; et `particles.nksl` (10/05, commit « Vulkan renderer ») porte le **layout PBR générique** (`aPos/aNormal/aTangent/aUV/aUV2/aColor`, `uObject.model * aPos`) — ce n'est pas un shader de particules.

📌 **Et six champs de `NkEmitterDesc` sur 24 ne sont jamais lus** : `texture`, `blend`, `coneAngle`, `loop`, `simMode`, `worldSpace`. La surface d'auteur promet plus que le système ne tient — *déclaré, pas livré*, comme les 108 widgets dont 2 peignent.

> 🔑 **Ce que ça change à la question.** Le chantier n'est ni « écrire des particules » (la simulation existe et tourne : 401 vivantes), ni « corriger un bug » : c'est **finir un renderer de particules dont on n'a que la moitié CPU**. Trois pièces à écrire, dans cet ordre, chacune avec son témoin : (a) le **tick** dans `NkRendererImpl` (une ligne, et le pont Noge cesse de mentir) ; (b) **un vrai shader de particules** — layout `NkVertexParticle`, expansion des coins **dans le vertex shader** à partir de `aSize` et du right/up caméra (pas de geometry shader : WebGL2 n'en a pas, et c'est le chemin Apple) — attaché aux trois pipelines ; (c) la **texture** et le **mélange** honorés. **Chiffrage : ~1,5 j.** 🚫 **Non lancé** — c'est une décision.

⚠️ **Ce que la mesure ne donne PAS** : le coût par particule (aucun chronomètre posé — inutile tant que rien ne se dessine) et la limite réelle de `maxParticles`. Ils se mesureront **après** (b).

🧰 **La sonde reste dans `Demo3D.cpp`, sous `NK_VFX_PROBE=1`** : elle reproduit les quatre nombres en une commande, sans rien changer pour qui ne pose pas la variable. Un contrôle positif CPU (expansion des coins) a été **préparé et non appliqué** : sans shader, il n'aurait rien prouvé.

### ✅ 2026-09-04 — **ELLES RENDENT.** Quatre manques, pas trois : le quatrième était la passe elle-même

`Captures/noge_particules_2026-09-04.png` — une gerbe de particules additives,
`renderdemo` OpenGL. **Le témoin est le compteur qui VARIE avec le sujet** :

| état | vivantes | `Draw:` | `Tris:` | écart |
|---|---:|---:|---:|---:|
| sans particules | 0 | 1093 | 489 588 | — |
| frame 60 | 167 | **1094** | 489 916 | **+328** ≈ 167 × 2 |
| frame 170 | 502 | **1094** | 490 542 | **+954** ≈ 502 × 2 |

`Draw` monte de **un** (un appel par émetteur, constant — c'est juste) et `Tris`
suit le nombre de particules, **deux triangles chacune**. *Un compteur qui ne
varie pas avec le sujet ne mesure pas le sujet ; celui-ci varie.*

**Ce qu'il a fallu — et le quatrième manque n'était pas dans mon relevé d'hier :**

| # | manque | correctif |
|---|---|---|
| 1 | aucun pipeline VFX n'avait de **shader** | `particles.vert.nksl` + `particles.frag.nksl` écrits, `LoadOrCompileVF("Particles")`, `pd.shader` + `pd.vertexLayout` (layout `NkVertexParticle`, stride 32) |
| 2 | quads d'**aire nulle** | **six** sommets par particule (la topologie est `TRIANGLE_LIST` : quatre faisaient un triangle et un orphelin), coins expansés **dans le vertex shader** depuis `aUV`/`aSize` et le repère caméra tiré de `uCam.view` — **pas de geometry shader** : WebGL2 n'en a pas, et le Web est le chemin Apple |
| 3 | personne n'appelait **`Update`** | la sonde le fait côté application, comme le faisait le legacy — le renderer n'a **ni `dt` ni caméra** à lui, lui en donner est une décision à part |
| 4 | 🔴 **la passe `VFX` du graphe avait un corps VIDE** — `(void)cmd;` sous un commentaire affirmant *« VFX flush intégré par le sous-système VFX »*, et `NkVFXSystem::Render` n'avait **aucun appelant** | la passe appelle `mVFX->Render(cmd, …)` |

> 🔑 **Le quatrième ne s'était pas vu hier, et c'est structurel** : les manques
> 1 et 3 le masquaient. Sans tick, `aliveCount == 0` court-circuitait le dessin ;
> sans shader, le pipeline était invalide de toute façon. **Trois défauts
> empilés, chacun cachant le suivant** — on ne les découvre qu'en les retirant un
> par un, et chaque retrait doit être mesuré, sinon on croit avoir fini.
> *Et deux commentaires affirmaient le contraire du code* (celui de la passe,
> celui du pont Noge) : un commentaire n'est pas une preuve d'exécution.

⚠️ **Bornes, dites nettes.** (a) La **texture** et le **mélange** par émetteur ne
sont toujours pas honorés — `NkEmitterDesc::texture` reste non lu ; le fragment
dessine un disque doux, pas un sprite. (b) Le pipeline ne déclare **pas** de
`descriptorSetLayouts` : le vertex shader lit `uCam` par le chemin **aplati de
GL**. Vérifié sur OpenGL ; **Vulkan demandera le layout global**, non fait, nommé.
(c) Le **coût par particule** n'est toujours pas chiffré.

### 🔎 CE QUI RESTE À MESURER AVANT DE PROPOSER QUOI QUE CE SOIT

Je **n'ouvre pas** ce chantier sans ces trois réponses, faute de quoi je
proposerais des fonctionnalités par-dessus un système que je n'ai pas vu tourner :

1. **est-ce que ça REND ?** Le code existe et il est appelé — ça ne prouve pas
   une image. *C'est exactement la distinction qui a coûté la journée d'hier :
   « ça compile » ≠ « ça tourne ».* Il faut une capture d'un émetteur vivant ;
2. **quelle est la limite réelle ?** `maxParticles = 1000` par défaut, montage
   CPU par image : le coût par particule n'a jamais été chiffré ;
3. **que manque-t-il pour un jeu ?** Les candidats visibles à la lecture :
   collision des particules, tri par profondeur pour la transparence, sous-émetteurs.
   **Aucun n'est à proposer avant d'avoir vu l'existant à l'œuvre.**

🚫 **Rien n'est commencé, et c'est délibéré** : le mandat disait de mesurer
d'abord. La mesure change la question — de « faut-il écrire des particules ? »
à « que manque-t-il à celles qui existent ? », et la seconde ne se répond pas
sans les faire tourner.


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
