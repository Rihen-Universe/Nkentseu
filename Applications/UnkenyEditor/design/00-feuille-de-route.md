# Unkeny et UnkenyEditor — feuille de route

### Document 00 — de l'état mesuré à « la totale »

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 29/09/2026 sur la branche `physic2d-editeur-ue5` (dernier commit lu :
> `b1c5e0643`), à partir de la vision de Rihen, mot pour mot :
> *« Unkeny est là pour la conception de jeux de différente nature, des jeux de
> plateau au jeu de plateforme au RPG, bref la totale ; pareil côté simulation
> physique, IA et bien plus encore. »*
>
> **Méthode.** Ce document a été écrit **en lisant le code**, sans rien compiler ni
> exécuter. Les chiffres de bancs (37/37, 14/14, 22/22…) sont ceux **écrits dans
> les messages de commit** ; les comptes de lignes et de cas viennent de `wc` et
> de `grep`. Ce qui n'a pas pu être vérifié est dit à l'endroit où il sert, et
> regroupé au § 7.
>
> **Compagnon :** `01-scripts-blueprint-et-cpp.md` (les deux langages de script,
> en cours de rédaction par un autre agent). Ce document-ci ne tranche **rien** sur
> les scripts : il leur réserve une place (palier U4) et renvoie au 01.
>
> ⚠️ **Travail en cours pendant la rédaction.** Un autre agent modifie
> UnkenyEditor dans le même dossier (arbre de travail non commité : clic de
> sélection, menu contextuel, gizmos, accrochage, origine du monde). L'état
> « éditeur » ci-dessous distingue ce qui est **commité** (✅) de ce qui est
> **en cours** (🔶). Ne pas relire ce document comme un constat figé sur ces
> points-là.

---

## À ne pas oublier — les demandes explicites de Rihen

Chaque demande a un numéro, un palier, et un état. Une demande qui disparaît de
ce tableau est une demande perdue : on la **raye** avec sa date et sa raison, on
ne l'efface pas.

| # | demande | palier | état au 29/09 |
|---|---|---|---|
| **R1** | Unkeny conçoit des jeux **de toute nature** : plateau, plateforme, RPG, « la totale » | U2 → U9 | 🔶 un seul genre joué sur Unkeny (le bac à sable physique) |
| **R2** | idem côté **simulation physique**, **IA** « et bien plus encore » | U2, U8, U9 | 🔶 physique riche ; IA **absente** d'Unkeny |
| **R3** | l'éditeur est **dans le style UE5, appliqué à la 2D** | U1 | ✅ disposition UE5 (`b1c5e0643`) |
| **R4** | **barre de titre maison** : menus **sur la ligne du titre**, onglets de scène **dessous** | U1 | ✅ commité ; ⚠️ déplacer / agrandir / redimensionner par la barre et les bords **non essayés** (message de `b1c5e0643`) |
| **R5** | **viewport plein** | U1 | ✅ « viseur qui prend tout le panneau » (`b1c5e0643`) |
| **R6** | **molette pressée = panoramique** | U1 | 🔶 au commit, bouton droit **ou** milieu déplacent la vue (`NkEditeurVue.cpp:10-13`) : le droit doit être rendu au menu contextuel |
| **R7** | **clic gauche = sélection** (dans le vide : plus rien) | U1 | 🔶 `NkEditeurCliquerSelection`, témoin (e13), non commité |
| **R8** | **glisser = déplacer** | U1 | 🔶 en édition seulement, témoin (e15), non commité |
| **R9** | **clic droit = menu contextuel** (sur une entité / dans le vide / « Ajouter ici ») | U1 | 🔶 `NK_CTX_ENTITE`, `NK_CTX_VIDE`, `NK_AJOUTER_ICI`, non commité |
| **R10** | **gizmos déplacement / rotation / échelle, touches W / E / R** | U1 | 🔶 Q/W/E/R et accrochage (Ctrl inverse) apparaissent dans `NkEditeurChrome.cpp:255,863` et le banc (e18), non commité |
| **R11** | **repère d'axes en bas à gauche** du viseur | U1 | 🔴 aucune trace trouvée (grep) au moment de la lecture |
| **R12** | **axes et origine du monde** visibles | U1 | 🔶 `NkDessinerGrille` trace déjà des axes ; « l'origine du monde » arrive dans `NkEditeurViseur.cpp:55`, non commité |
| **R13** | **F = cadrer la sélection, même hors cadre** | U1 | 🔶 `NkEditeurCadrerSelection` (e14) **centre sans changer le zoom** : une sélection plus grande que la vue ne sera pas « cadrée » — à décider (§ 5, U1) |
| **R14** | **deux langages de script** : Blueprint **et** C++ rechargé à chaud | U4 | 🔶 S0–S2 faits le 01/10 (composant multi-scripts, hôte, table C, VM, compilateur, page du graphe, C++ rechargé à chaud, livraison) : document 01 § 14 ; restent S3, S4 complet, S6 mobile/Web, S7 |
| **R15** | **les corps mous peuvent être des personnages, ou des parties de personnage**, déplaçables **à la souris, au clavier, à la manette ou au tactile** | U2 | 🔴 la souris seule existe (saisie en jeu) ; analyse au § 4 |
| **R16** | **une fois le jeu terminé, le construire pour la bonne plateforme depuis l'éditeur** (Windows, Android, Web, puis HarmonyOS, Linux, macOS / iOS) — demande du 30/09 | U5 | 🔶 fondations lancées le 30/09 (branche `comble/livrer-u5` : joueur autonome + fenêtre Construire) |
| **R17** | **tout logo existe en version sombre ET en version claire** (NKCraft, et l'icône de chaque jeu construit) — demande du 30/09 | U5 | 🔶 fait pour NKCraft (`c7b1f814c`) ; à appliquer à l'icône des jeux construits |
| **R18** | **une IA intégrée, entièrement ouverte** : un modèle branché comprend Unkeny dès sa première connexion et agit sur tout (création, conception, discussion, GDD) ; garde-fous : annulation de tout, confirmation de l'irréversible — demande du 30/09 | U4 | ✅ 01/10 (branche `unkeny/panneau-ia`, à intégrer) : **document 05** — l'onglet **IA** du groupe Détails \| Monde (détachable, repliable, disposition retenue ; Fenêtre > IA, Ctrl+I) ; les fournisseurs en UNE couche partagée (`Kernel/System/NKConverse/NkConverseFournisseurs.h` : Ollama, compatible OpenAI, API Anthropic, CLI Claude, processus local ; liste des modèles par le serveur ; « Tester la connexion » ; flux ; clés hors du dépôt) ; 21 outils qui sont les commandes de l'éditeur ; message système ENGENDRÉ par l'éditeur ; chaque action par l'historique (+ journal des fichiers), confirmation de l'irréversible ; banc `--banc-ia` (41 témoins, 4 contre-épreuves), captures `References\Captures\ia-unkeny\` ; Qwen par Ollama sur l'autre PC : document 05 § 3 ; reste : essai avec le vrai Qwen, NKCode et NKCraft sur la même couche |
| **R19** | **fidélité à Unreal 5 pour toute la famille**, chaque application avec sa touche — demande du 01/10 | U1 | 🔶 document 02 ; Content Browser en cours dans NKEditorKit |
| **R20** | **appareils réalistes, formes d'écran, caméras (découpes), rotation exacte, zone sûre** (NKWindow en est la source) — demandes du 30/09 et du 01/10 | U5 | 🔶 document 03 ; en cours |
| **R21** | **des cinématiques** dans Unkeny — demande du 01/10 | U7 | 🔴 rien dans Unkeny ; Noge a un séquenceur (`Engine/Noge/Sequencer`, banc `NkSequenceCheck`) ; NKAnima a le modèle d'édition d'une frise (`Edit/NkAnimationEditor.h`). Piste : un modèle de séquence descendu dans le noyau, une frise partagée dans NKEditorKit, rendu en images puis MP4. **Pistes extensibles** (Rihen, 01/10 : « avec le temps on va ajouter d'autres propriétés ») : position, rotation, échelle et couleur d'abord, mais **toute propriété d'un composant** doit pouvoir devenir une piste par la réflexion de NKECS, avec une interpolation par type (nombre, vecteur, couleur, et « par paliers » pour booléen, énumération, référence d'asset) ; ajouter une propriété ne demande jamais de réécrire le séquenceur | — 01/10 : pistes extensibles FAITES (`Unkeny/Anim/NkUnkenyProprietes.h` : noyau, composants décrits, composants réfléchis par NKECS ; interpolation par genre ; `.nkanim` v4) et frise partagée FAITE (NKEditorKit, générique : NKScena s'en servira) ; reste la séquence (plusieurs objets, caméras, coupes) et le rendu en images / MP4
| **R23** | **un Launcher du moteur** (projets, versions, modèles, actualités) qui porte aussi les variantes du logo (fonds d'écran, déclinaisons) — demande du 01/10 | U5 | 🔴 n'existe pas ; la spécification « Aetherion » (Nogee/design 1 à 3) prévoit un Launcher |
| **R24** | **dans toutes les applications** (comme NKCraft) : boutons **Tutoriel** (enregistrer l'application) et **Capture** (la scène ou toute l'application) ; enregistrer en suite d'images ou en vidéo (formats YouTube, TikTok…) ; **diffuser en direct** (YouTube et autres) sans logiciel tiers — demande du 01/10 | U5 | 🔶 NKCraft a « Tutoriel » et la capture (`Shell/NkModelerChrome.h`, `main.cpp`) ; NKMedia a un encodeur H.264 maison (`Codecs/Video/H264/NkH264Encoder.cpp`) et Opus ; à descendre dans NKEditorKit. Le direct (client RTMP, envoi continu, audio accepté par les plateformes) est à écrire |
| **R25** | **un terminal intégré** qui ouvre le vrai shell de la plateforme (PowerShell, cmd, Git Bash, WSL, bash, zsh…), autant qu'on veut, **beau et clair** comme le reste de la famille — demande du 01/10 | U1 | 🔶 en cours : le moteur de NKCode (`NkPty`, `NkTerm`) descend dans NKEditorKit, dessin refait |
| **R26** | **la caméra comme composant** : suivre le joueur (cible = une entité, réglable par script), lissage, zone morte, anticipation, bornes du niveau, zoom, tremblement, plusieurs caméras et passage de l'une à l'autre ; **la miniature (minicarte) comme propriété de la caméra** (coin, taille, cadre, calques montrés) — demande du 01/10 | U2 | 🔶 le moteur a `Vues/NkUnkenyVues.h` : `NkVuePosee` (suivi doux d'un point, bornes), plusieurs vues (écran partagé, minicarte en rendu direct) et miniatures hors écran ; **rien n'est exposé** comme composant dans la scène ni dans l'éditeur ; la cible est un point, pas une entité ; pas de zone morte, d'anticipation ni de tremblement ; « par script » attend les scripts (R14) |
| **R27** | **des panneaux déplaçables** : chaque panneau (Placer des acteurs, Outliner, Détails, Contenu…) se glisse et se pose en onglet à gauche, en bas, à droite, ou flotte — demande du 01/10 | U1 | 🔶 NKEditorKit a déjà le docking (`NkEditorShell`, `NkEditorPanel`, utilisés par NKCode, NKCraft, NKUIDesign, NkAnimaEditor) ; UnkenyEditor a sa propre disposition fixe : le faire passer sur la coquille partagée |
| **R28** | **double-cliquer sur un sprite ou une forme ouvre son éditeur** avec un panneau **physique** pour l'ajuster (collision, masse, matériau, calques), comme l'éditeur de maillage / d'asset physique d'Unreal 5 — demande du 01/10 | U1 | 🔴 à faire ; s'appuie sur l'édition des collisions (en cours) |
| **R29** | **l'aimant** : coller aux sommets, aux arêtes et aux faces des autres objets, en édition comme en déplacement — demande du 01/10 | U1 | 🔶 confié à l'agent des formes et des collisions |
| **R30** | **rigging et skinning 2D** : poser des os sur un personnage, déformer l'image (maillage pondéré), IK 2D, animer les os sur la frise, mouvement secondaire par les corps mous (écharpe, cheveux) — demande du 01/10, utile au Dernier Feu | U2 | 🔶 NKAnima a le squelette (`Skeleton/NkSkeletonDef.h`), les clips, le reciblage et les poses ; l'éditeur d'os 2D, la déformation de maillage, l'atlas de sprites et la pelure d'oignon sont **spécifiés** (NkAnimaEditor, document 04 §12-13, espace « 2D ») mais pas écrits ; rien dans Unkeny |
| **R31** | **un composant de maillage 2D** (« mesh renderer / mesh editing ») avec **sa fenêtre d'édition** : éditer les sommets, découper en parties, et donner **une physique indépendante à chaque partie** si on le souhaite — demande du 01/10 | U1 | 🔴 les formes 2D existent (polygone libre, collisionneurs) ; pas de fenêtre de maillage ni de parties physiques |
| **R32** | **Nogee ressemble exactement à UnkenyEditor**, puis reçoit ses propriétés et sa disposition propres à la 3D ; **NkAnimaEditor est d'abord une application 3D comme NKCraft**, spécialisée animation et VFX — demande du 01/10 | U1 | 🔶 en cours (01/10) : Nogee puis **NkAnimaEditor** prennent exactement l'apparence d'UnkenyEditor, vue 3D au centre, pièces communes dans NKEditorKit ; **NKScena** aussi (demande du 01/10) : sa condition de naissance (une séquence se sauve et se relit, `.nkseq`, témoin f7 de NkSequenceCheck) est remplie, elle naîtra avec ces pièces communes et la frise partagée (R35) |
| **R33** | **chaque asset s'ouvre** : double-cliquer une texture, une police, un son, un prefab ou un contrôleur ouvre son onglet de réglages ou ses détails modifiables (comme une scène) ; **glisser un prefab** dans la vue ou l'Outliner crée l'instance ; **tous les réglages dans des cartes** de composants (rien hors cadre) ; « Ajouter un composant » propose collisionneur (au choix), corps rigide, forme, animation, animateur, émetteur, lumière, son ; **le panneau Placer des acteurs se replie** ; **l'éclairage de la scène s'allume et s'éteint** clairement — demande du 01/10 | U1 | 🔶 en cours |
| **R34** | **les effets** (feu, étincelles, fumée, neige) ne tournent qu'**en jeu** (aperçu en édition au choix), « jouer au démarrage », et se **pilotent** (jouer, arrêter, pause) — par C++ maintenant, par script ensuite — demande du 01/10 | U2 | 🔶 en cours |
| **R35** | **les pages Animation et Animateur** : une page Animation avec sa **frise** (pistes, images-clés, courbes, lecture), une page Animateur avec le **graphe** des états où l'on **relie les animations** entre elles (transitions, conditions, paramètres) ; Rihen, 01/10 : « tous doivent exister » (avec l'IA, les scripts et Blueprint, l'éclairage, le rigging et le skinning 2D, le maillage 2D) | U2 | ✅ 01/10 (branche `editorkit/animation-animateur`, à intégrer) : deux composants PARTAGÉS dans NKEditorKit — la frise (`Components/NkTimelineModel.h`) et le graphe d'états (`Components/NkStateGraphModel.h`) ; dans UnkenyEditor, deux onglets de document : **Animation** (`Editeur/NkEditeurPageAnimation.cpp`, un `.nkanim`) et **Animateur** (`Editeur/NkEditeurPageAnimateur.cpp`, un `.nkanimctl`) ; ouverts par Fenêtre > Animation / Animateur ou `NkEditeurOuvrirAnimation(chemin)` / `NkEditeurOuvrirAnimateur(chemin)` ; banc « BANC ANIMATION », captures `References\Captures\animation\` |
| **R36** | **les greffons** : un développeur conçoit ses propres outils et modules (panneaux, cartes de Détails sur mesure, types d'asset et leur page, importeurs, commandes, outils de la vue, composants et systèmes de jeu) pour ce que le moteur ne fait pas ; premier greffon : le **créateur de créatures-gelées** du Dernier Feu — demande du 01/10 | U4 | 🔴 conçu (document 04) ; s'appuie sur l'hôte C++ et la VM des scripts (S0–S2, en cours) |
| **R22** | **éditer les animations dans l'éditeur** (découper une planche de sprites, clips, contrôleur `.nkanimctl` en graphe, animation de propriétés par images-clés) — demande du 01/10 | U2 | 🔶 01/10 : contrôleur `.nkanimctl` en graphe et animation de propriétés par images-clés FAITS (R35) ; un état de l'Animateur qui désigne une animation la joue (`NkClipProprietes2D`) ; reste : **découper une planche de sprites** (clips de `NkAnimSprite2D` éditables dans une page) |

Et les **règles héritées** qu'aucun palier n'a le droit de casser :

| règle | origine |
|---|---|
| **Unkeny rend par NKCanvas, jamais par NKRenderer** : les deux sont exclusifs | `Engine/Unkeny/README.md`, `Unkeny.jenga` |
| **Unkeny ne réécrit rien de ce que le dépôt porte déjà** : avant d'écrire un mécanisme, chercher qui le porte, en commençant par la couche du dessous | `Engine/Unkeny/README.md` |
| **L'éditeur simule les zones sûres** pour voir le débordement **avant** de déployer | Rodolf, 2026-08-18 (`NkEditeurAppareils.h`) |
| **Toute action de l'éditeur est une fonction du modèle, éprouvée par `--selftest`**, jamais écrite dans un dessin de panneau | `NkEditeurActions.h` |
| **Un moteur ne contient pas son outil** : l'éditeur vit dans `Applications/` | `Engine/Unkeny/README.md` |
| **Ne compte comme livré que ce qui est exercé** | `Applications/Nogee/design/04`, § 6 |

---

## 0. En une phrase

**Unkeny sait déjà très bien simuler (corps rigides couplés à des corps mous,
fluides, sable, tissus, cordes) et sauver ce qu'il simule ; il ne sait pas encore
faire JOUER : aucune entrée n'arrive au jeu, aucune caméra ne suit, aucune carte
de tuiles ne se dessine, et aucun jeu de genre n'a été construit dessus. La feuille
de route consiste à le faire jouer (U2), à lui donner un monde (U3), une logique
éditable (U4) et un chemin vers les appareils (U5), puis à prouver chaque genre par
un petit jeu d'exemple (U6 → U10).**

---

## 1. Les cinq constats qui orientent tout

1. **Unkeny n'a que deux consommateurs, et aucun jeu.** Seuls `Physic2D` et
   `UnkenyEditor` lient `"Unkeny"` (grep des `.jenga`). Les trois jeux de plateau
   sur lesquels ses briques ont été **mesurées** (NkDames, NkEchecs, NkLudo) ne
   l'ont **jamais adopté** : leurs copies de `NkDansRect`, des aides de texte, des
   animations et de l'IA sont toujours en place. Conséquence mesurée :
   `NkActions`, `NkSieges`, `NkChemin`, `NkPlanGrille`, `NkVuePosee`,
   `NkMiniature` ont **zéro appelant** dans tout `Applications/`. Le README
   d'Unkeny met lui-même en garde contre le « système dormant » : on y est.
2. **La carte de tuiles est une coquille.** `NkCarteTuiles` stocke et interroge,
   mais **rien ne la dessine**, **rien ne la sauve** (absente de
   `NkUnkenySauvegarde.cpp`), et `ConstruireCorpsStatiques`, annoncée dans
   l'en-tête (`NkUnkenyTuiles.h:20-24`), **n'existe pas** dans le `.cpp`.
   L'éditeur en crée une de 40 × 24 à l'ouverture (`NkEditeurApp.cpp:238-240`)
   qui ne se voit nulle part. Aucune autre carte de tuiles n'existe dans le
   dépôt, ni aucun import Tiled / LDtk.
3. **La physique est riche mais 3D dans l'âme.** NKPhysics est un monde 3D dont
   les formes 2D vivent à z = 0 ; `NkPhysicsConfig::enable2D` n'est **lu nulle
   part** (`NkPhysicsTypes.h:45`). Pas de raycast 2D au niveau physique, un
   `Raycast2D` de NKCollision exact seulement pour le cercle et sans l'arbre
   (`NkCollisionWorld.cpp:462-491`), **CCD en 3D seulement** (`:578`), joints à
   API 3D **jamais testés en 2D**, aucun contrôleur de personnage. En face,
   `NkParticules2D` (1 647 lignes, PBD/XPBD, neuf matériaux, couplage aux rigides
   dans les deux sens) est la brique la plus originale du dépôt pour un jeu 2D.
4. **L'IA existe, mais pas celle d'un jeu.** Aucun arbre de comportement,
   blackboard, GOAP ni utility AI dans tout le dépôt. Ce qui existe : NKRL
   (Q-learning, DQN, PPO sur grilles), NKAgent, NKEmbodied, NKCivilization, NKEvolve
   — tous sauf NKEvolve tirent **NKTensor, donc NKRHI et NKSL** au lien
   (`NKTensor.jenga:32-33`), même pour un Q-learning tabulaire. Les IA des jeux
   de plateau sont trois heuristiques recopiées (« ce n'est PAS une recherche »),
   avec `NkNiveauIA` dupliqué trois fois. Rien n'est branché dans Unkeny.
5. **Les briques d'appareil sont là, pas branchées.** La manette existe
   (`NkGamepadSystem`, backends réels Win32, Xbox, Linux, Android, Web ; **bouchons**
   macOS, iOS, UWP, HarmonyOS), le tactile porte 32 points, Jenga sait produire
   APK/AAB, `.hap`, `.ipa`, wasm, `.app`, msi… Mais **aucune application Unkeny ne
   lit la manette**, l'éditeur ne traduit aucune entrée en action pendant « Jouer »,
   et Unkeny n'est **prouvé que sous Windows** (plus une construction XLib sous
   Xvfb, `3d710fab9`).

---

## 2. Inventaire de l'existant (U0)

Légende : ✅ prouvé (compilé, lié **et** exercé par un banc ou une appli) ·
🔶 partiel · 🔴 écrit sans appelant, ou spécifié seulement.

### 2.1 Unkeny — ce qui est exercé

`Engine/Unkeny`, 35 fichiers, 7 085 lignes. Banc du moteur :
`NkUnkenyLancerBanc()` (`Banc/NkUnkenyBanc.cpp`), **37 témoins**, « 37/37 »
sous Windows 11, clang-mingw, Debug (message de `b1c5e0643`).

| brique | fichier | ce qu'elle fait | éprouvée par | état |
|---|---|---|---|---|
| **Scène** | `Scene/NkUnkenyScene.h` | monde NKECS ; physique **facultative** (`NkSceneConfig::physique = false` par défaut) ; pas fixe 1/60, plafond de 5 pas par trame ; téléportation explicite, seul sens transform → physique | bancs Unkeny, Physic2D (u1–u10), éditeur (e1–e12) | ✅ |
| **Composants** | `Scene/NkUnkenyComposants.h` | `NkTransform2D`, `NkSprite2D` (atlas, couche), `NkCollisionneur2D` (cercle, boîte, capsule ; couches et masque ; déclencheur), `NkCorps2D` (identifiant, jamais la position), `NkCorpsMou2D`, `NkEtiquette`, `NkVitesse2D` | idem | ✅ |
| **Logique de jeu** | `NkScene::AjouterSysteme` | planificateur **séquentiel** à deux phases (`NK_PAS_FIXE` avant la physique, `NK_TRAME` après) ; **pas** `ecs::NkScheduler`, qui impose des fils incompatibles avec le Web | (j1)–(j3) | ✅ |
| **Contacts et zones** | `NkScene::Contacts()` | chocs et entrées / sorties de zone du dernier pas, **en entités** | (j4), (j5) | ✅ pour les **rigides seulement** (§ 4) |
| **Photo** (Jouer / Arrêter) | `NkScene::Photographier`, `Restaurer`, `PhotographierAussi<T>` | refait la scène à l'identique ; un composant du jeu y entre par déclaration (32 types au plus) | (u5), (u5d), (e5) | ✅ ; les POIGNÉES d'entité changent à la restauration, mais depuis le 30/09 l'**identité stable** (`NkIdentite2D`, `Uid` / `EntiteParUid`), les parents et les références `NK_ENTITE` la traversent — témoins (i1), (h7), (i3) |
| **Sauvegarde** `.nkscene` | `Scene/NkUnkenySauvegarde.h` | JSON par NKSerialization ; composants d'Unkeny champ par champ ; état des rigides ; tout le monde de particules ; textures **par leur nom** | (s1)–(s4), Physic2D (u10), (e7) | ✅ ; **version 2** depuis le 30/09 : identités, hiérarchie, composants du jeu DÉCRITS écrits champ par champ (portables, relus après l'ajout d'un champ), sons / textures / prefabs par leur nom ; un composant non décrit reste en octets ; la v1 se relit à l'identique (témoins (v1), (r1), (a7)) |
| **Rendu** | `Rendu/NkUnkenyRendu.h` | sprites triés par couche, hors-champ non dessiné, compteurs ; formes pleines ; contours des collisionneurs ; grille et axes | (t4)–(t6) | ✅ |
| **Rendu de la matière** | `Rendu/NkUnkenyRenduParticules.h` | ballon, gelée, tissu, fluides, sable, atomes, corde ; modes éclairé, filaire, contraintes, vitesse | Physic2D, éditeur (à l'œil) | 🔶 aucun témoin d'image |
| **Textures** | `Rendu/NkUnkenyTextures.h` | NKImage → RGBA → téléverseur branché par l'appli ; chargement sans GPU ; renvoi après perte de contexte | (t1)–(t3), (t9) | ✅ |
| **Animation d'images** | `Anim/NkUnkenySpriteAnim.h` | clips contigus, trois modes de lecture, événement à une image, temps accumulé (pas d'indice) | (t7), (t8) | ✅ ; son en-tête renvoyait à « NKAnimation » et à une « HFSM » alors **plate** — corrigé le 2026-09-29 : il renvoie à **NKAnima**, devenu hiérarchique, et à `NkAnimateur2D` |
| **Son** | `Son/NkUnkenySon.h` | sons chargés ou fabriqués ; lecture à plat ou placée (panoramique à l'écran, atténuation hors champ) ; coupe les voix des entités détruites | (a1)–(a6), moteur à sortie nulle | ✅ ; n'utilise **ni** bus, **ni** musique, **ni** flux de NKAudio |
| **Catalogue de simulation** | `Simulation/NkUnkenyActeurs.h` | 14 acteurs (ballon, blob, slime, gelée, eau, miel, sable, cristal, disque, tissu, corde, pont, caisse, balle), textures et bruitages fabriqués par programme | Physic2D, éditeur (e2) | ✅ |

### 2.2 Unkeny — écrit, sans appelant

| brique | fichier | ce qu'elle promet | ce qui manque | palier qui la branche |
|---|---|---|---|---|
| **Actions d'entrée** | `Entree/NkUnkenyActions.h` | le jeu parle d'actions en axes −1..1, trois états, direction normalisée | **zéro appelant** ; aucune source (clavier, manette, doigt) | **U2** |
| **Vues posées, miniature** | `Vues/NkUnkenyVues.h` | suivi amorti **indépendant du pas** (`NkUnkenyVues.cpp:21-27`), bornes de carte, miniature hors écran | zéro appelant, zéro témoin ; pas de secousse ; `NkUnkenyCameraReelle.h`, cité à `:31`, **n'existe pas** | **U2** (suivi), **U7** (miniature de sauvegarde) |
| **Carte de tuiles** | `Monde/NkUnkenyTuiles.h` | couches, parallaxe, nature des tuiles, conversions case ↔ monde | dessin, sauvegarde, corps statiques, édition, témoins | **U3** |
| **Sièges** | `Jeu/NkUnkenySieges.h` | humain / IA par place, modes comme raccourcis jamais stockés ; `NK_RESEAU` prévu | zéro appelant | **U6** (et U10 pour le réseau) |
| **Machine à états d'animation** | `Anim/NkUnkenyAnimateur.h` (2026-09-29) | `NkAnimateur2D` : paramètres (vitesse, au sol, saut…) → la **HFSM de NKAnima** choisit l'état → le clip de `NkAnimSprite2D` ; un modèle partagé, l'état de chaque entité dans son composant ; modèle « plateforme » fourni (Sol : idle / marche, Air : saut / chute) ; photographié et sauvé (`.nkscene`, clé `NkAnimateur2D`) ; modèles lisibles d'un `.nkanimctl` (extension du 30/09, décision de Rihen) | (n1)…(n10) | ✅ ; section Détails en lecture (état, paramètres modifiables) ; ni « + Ajouter », ni éditeur de graphe |
| **Chemin animé** | `Anim/NkUnkenyChemin.h` | déplacement le long d'étapes, état immédiat et animation purement visuelle | zéro appelant ; trois copies vivent dans les jeux de plateau | **U6** |
| **Mise en page, widgets** | `Ui/NkUnkenyGeometrie.h`, `NkUnkenyWidgets.h` | `NkPlanEcran` (bandeau + carré + bandes), `NkPlanGrille`, boutons, voile de fin de partie | zéro appelant hors Unkeny | **U6**, **U7** |

📌 **Décision proposée** (même règle que la D5 du document 04 de Nogee) : chaque
brique dormante est **branchée par le palier qui en a besoin, ou retirée** à ce
palier. On ne la garde pas « pour plus tard » : un en-tête sans appelant se lit
comme une fonctionnalité livrée.

### 2.3 UnkenyEditor — ce qu'il sait faire

`Applications/UnkenyEditor`, environ 5 100 lignes à la lecture (arbre de travail,
en mouvement). Base : `renderer::NkCanvasGuiApp`, **plus** de `NkEditorShell`
(`b1c5e0643`). Banc : `NkEditeurLancerBanc()`, lancé par `--selftest` après celui
d'Unkeny, « 14/14 » au commit ; (e13)–(e18) s'y ajoutent dans l'arbre de travail.

| domaine | ce qui existe | état |
|---|---|---|
| **Chrome** | fenêtre sans cadre, menus Fichier / Édition / Fenêtre / Aide sur la ligne du titre, boutons de fenêtre ; onglets de scène dessous (✕, point « modifiée ») ; barre d'outils ; barre d'état ; cloisons déplaçables ; menus peints dans `dlOverlay` | ✅ (déplacement et redimensionnement de la fenêtre non essayés) |
| **Outliner** | arbre du kit (`NkTreeViewModel`), ordre stable, type de chaque entité, recherche | ✅ ; **arbre** depuis le 30/09 : chaque entité sous son parent, glisser pour rattacher, lâcher dans le vide pour détacher, clic droit = menu de l'entité (dont « Créer un prefab ») — glisser NON essayé à la vraie souris |
| **Détails / Monde** | une section par composant (Transform, Sprite, Collisionneur, Corps rigide, Corps mou, Source sonore, Animation, et depuis le 2026-09-29 Animateur — sans « Retirer » ni place dans « + Ajouter »), retirable ; « + Ajouter » ; le Monde règle gravité, température, vent, frein de l'air, sous-pas | ✅ |
| **Tiroir** | Acteurs (le catalogue de simulation, glisser vers la vue) et Journal | ✅ ; ce n'est **pas** un navigateur de fichiers de projet |
| **Viseur** | viseur plein, grille, collisionneurs, liens, particules, vitesses ; profils d'appareils (6) et zone sûre en surimpression, paysage | ✅ |
| **Outils** | Sélection, Poser, Effacer, Saisir, Couteau (les deux derniers en jeu) | ✅ |
| **Gestes UE5** | clic de sélection, glisser, menu contextuel, gizmos Q/W/E/R, accrochage, origine du monde | 🔶 en cours (tableau des demandes) |
| **Jouer** | Jouer / Pause / Un pas / Arrêter (la photo rend la scène d'avant) | ✅ ; ⚠️ en jeu, **aucune entrée n'arrive au jeu** : les touches vont à NKGui (`NkEditeurApp.cpp:329`), la souris aux outils |
| **Fichier** | Nouveau, Ouvrir, Enregistrer **un** `scene.nkscene` dans le dossier de l'application ; confirmation « non enregistrée » par empreinte FNV-1a | 🔶 pas de projet, pas de « Enregistrer sous », pas de choix de fichier |
| **Absent** | annuler / refaire (aucune trace, grep), multi-sélection, import d'images, éditeur de tuiles, d'animation, d'UI, de graphes, construction et empaquetage | 🔴 |

### 2.4 Les modules du dépôt, pour « la totale »

« Branché » = présent dans `Unkeny.jenga` **et** appelé par Unkeny.

| module | où | ce qu'il fournit à un jeu 2D | maturité | branché ? |
|---|---|---|---|---|
| **NKPhysics** | `Kernel/Runtime/NKPhysics` (9,3 k lignes) | corps statiques, cinématiques, dynamiques ; drapeaux rotation fixe, sans gravité, déclencheur, CCD, sommeil ; joints distance, rotule, pivot (moteur, limites), soudure — **prismatique dans l'enum seulement** ; événements Contact/Trigger Enter/Exit (paires d'identifiants, sans point ni normale) ; `OverlapShape` 2D | `test_physics.cpp` : 61 CHECK **tous 3D** ; `test_particules2d.cpp` : 23 témoins ; « 225 / 7 » cité par `dfde7abcb` (7 échecs préexistants du vêtement) ; docs contradictoires (README « scaffold ») | ✅ corps et événements ; 🔴 joints |
| ↳ défaut probable | `NkRigidBody.h:129-163` | un **polygone ou triangle 2D dynamique** ne suivrait pas son corps (`NkTransformShape` ne les traite pas, le GJK 2D lit des sommets absolus) | déduit de la lecture, **non exécuté** | — |
| **NkParticules2D** | `NKPhysics/NkParticules2D.h` | voir § 4 | 23 témoins, Physic2D (u1–u10) | ✅ |
| **NKCollision** | `Kernel/Runtime/NKCollision` (3,7 k lignes) | 8 formes 2D (point, segment, cercle, capsule, triangle, boîte orientée, polygone convexe, **chaîne concave** pour le terrain) ; SAT, clipping à 2 points, GJK/EPA 2D ; arbre d'AABB ; couches et masque | 107 CHECK, dont **environ 14 lignes en 2D** | 🔶 via NKPhysics ; Unkeny n'expose que cercle, boîte, capsule |
| **NKNavigation** | `Kernel/Runtime/NKNavigation` (565 lignes) | un navmesh **sur le plan XZ** (raycast vertical, pente, obstacles AABB), A* sur les triangles, **sans** lissage funnel | pas de `tests/` (déclaré par le `.jenga`) ; NkNavCoreDemo : 11 Check | 🔴 ; ni grille A* 2D, ni champ de flux, ni évitement |
| **NKSimulation** | `Kernel/Runtime/NKSimulation` | rien : README et ROADMAP, aucun code | — | — |
| **NKRL** | `Kernel/AI/NKRL` | Q-learning, DQN, PPO ; environnements grille, clé-porte, atteinte 2D | NKRLTest : 5 tests (Q-learning et DQN à 100 %, PPO continu 26 %) | 🔴 ; tire NKTensor → NKRHI |
| **NKAgent** | `Kernel/AI/NKAgent` | agent, mémoire, perception **spécialisée NkGridWorld**, pile de sous-buts (pas du GOAP), personnalité, raisonnement LLM (≈ 148,7 s par décision) | NKAgentTest, NKAgentLLMTest | 🔴 ; tire NKTensor, NKInfer |
| **NKEmbodied** | `Kernel/AI/NKEmbodied` | capteurs, actionneurs, boucle à fréquence fixe (orientation robotique) | NKEmbodiedTest | 🔴 |
| **NKCivilization** | `Kernel/AI/NKCivilization` | agents sur grille en **système NKECS**, ressources, enregistreur `.nkciv`, rejeu, heatmap | 3 applis de test ; collision O(N²) ; « ça émerge » non atteint | 🔴 |
| **NKEvolve** | `Kernel/AI/NKEvolve` (443 lignes) | algorithme génétique, neuroévolution | NKEvolveTest, NKEvolveNNTest | 🔴 ; **le seul sans NKTensor** |
| **NKGraph** | `Kernel/Runtime/NKGraph` (en-têtes seuls) | substrat **unique** de graphe de nœuds : prises typées, validation, tri topologique, sous-graphes, `.nkgraph`, annuler / refaire par instantanés ; **pas d'interpréteur** (« on unifie l'autorat, jamais l'exécution ») | graphe de matériaux (NkMatGraphCheck, 26 cas) | 🔴 ; voir document 01 |
| **Blueprint de Noge** | `Engine/Noge/src/Noge/ECS/VisualScript/NkBlueprint.h` (909 lignes) | interpréteur naïf à pile, nœuds BeginPlay, Raycast, SpawnActor… | aucun test ; à **réaligner sur NKGraph** d'après la roadmap de NKGraph | 🔴 ; voir document 01 |
| **C++ à chaud** | `Noge/ECS/Scripting/NkScriptBridge` + `Applications/NkHotReloadDemo` | DLL compilée à l'exécution, copie fantôme, état capturé puis restauré | prouvé **sous Windows seulement** ; branche `dlopen` non prouvée ; vit **dans Noge** | 🔴 ; voir document 01 |
| **NKAnima** | `Kernel/Runtime/NKAnima` (≈ 6 k lignes) | clips et images-clés (dont `spriteFrame`, `uvOffset`, `opacity`, `BuildSpriteFlipBook`), courbes (EASE, BOUNCE, ELASTIC, BACK), mélange 1D/2D, machine à états **hiérarchique** (plate jusqu'au 2026-09-29), trajectoires Catmull-Rom, édition avec annuler / refaire | `test_selftests.cpp`, `test_hfsm.cpp` ; **Unkeny** l'inclut depuis le 2026-09-29 (`NkAnimateur2D`), aucune autre appli NKCanvas | 🟡 ; ses courbes et sa piste de sprite restent inutilisées par Unkeny ; compatible NKCanvas (Foundation seulement) ; squelette en matrices 4×4, pas de squelette 2D dédié |
| **NKVFX** | `Kernel/Runtime/NKVFX` (≈ 400 lignes) | un constructeur de maillage d'eau 3D | **dépend de NKRenderer** | ⛔ inutilisable dans Unkeny ; **aucun émetteur de particules 2D dans le dépôt** — 🔶 comblé le 2026-09-30 par `Unkeny/Effets` (branche `comble/lumiere-effets-2d`, voir U9) |
| **NKCanvas** | `Kernel/Runtime/NKCanvas` (≈ 30 k lignes) | backends GL / GLES / WebGL2, Vulkan, DX11, DX12, logiciel ; sprites, formes, lots de 262 144 sommets, découpe, 8 modes de fusion ; cibles de rendu ; coquille `NkCanvasApp` (zone sûre, densité, pause / reprise, surface perdue) | un test ; ≈ 40 applis | ✅ ; ⚠️ **shaders personnalisés sous OpenGL seulement** ; **Metal sans Renderer2D** (iOS ne dessine qu'en logiciel) ; ni éclairage 2D, ni post-effets, ni neuf-tranches — 🔶 l'éclairage 2D existe depuis le 2026-09-30 **au-dessus** de NKCanvas (`Unkeny/Rendu/NkUnkenyEclairage` : carte de lumière au processeur posée en `MULTIPLY`, sans shader ni cible hors écran) |
| **NKGui** | `Kernel/Runtime/NKGui` (≈ 24–27 k lignes) | mode immédiat, 133 widgets, mise en page, 35 jetons de thème, glisser-déposer ; documents `.nkgui` montés par `Doc/NkGuiMonteur.h` ; multilingue `NkGuiLangues` | NKGuiDrawTest (« 115/115 », non rejoué), Interact, Monte | ✅ pour dessiner ; 🔴 comme UI de jeu ; ⚠️ **pas de navigation au focus** clavier / manette |
| **NKUI** | `Kernel/Runtime/NKUI` | **déprécié le 16/08/2026** (`NKGui/README.md:3`) | un consommateur | ⛔ ne pas l'utiliser |
| **NKUIDesign** | `Applications/NKUIDesign` | l'éditeur d'UI de la maison, sur NKCanvas + NKGui, sans NKRenderer ; écrit et valide des `.nkgui` | 20 exemples ; fonctionnement non vérifié | — ; c'est **lui** qui fera les écrans de jeu (U7) |
| **NKFont / NKImage** | `Kernel/Runtime/NKFont`, `NKImage` | TTF / OTF / WOFF, SDF, crénage, 10 polices embarquées ; PNG, JPEG, BMP, TGA, HDR, QOI, GIF animé, WebP, SVG | pas de `tests/` pour NKFont ; `TestSVG.cpp` | ✅ ; ⚠️ **ni bidi, ni mise en forme complexe, ni texte riche** |
| **NKCamera** | `Kernel/Runtime/NKCamera` | la caméra **physique** de l'appareil et une pose AR | NKARDemo | — ; hors sujet pour la caméra de jeu (qui est `NkVue2D`) |
| **NKAudio** | `Kernel/Runtime/NKAudio` (≈ 21 k lignes) | 256 voix, bus Master / SFX / Music / Voice / UI avec effets et ducking, fondu enchaîné de musique, HRTF, Doppler, WAV / MP3 / OGG / FLAC / Opus, 7 backends dont WebAudio | 114 TEST ; NkAudioDemo, NkAudioECSDemo | ✅ lecture ; 🔴 bus, musique, flux |
| **NKNetwork** | `Kernel/System/NKNetwork` (≈ 19–20 k lignes) | sockets UDP / TCP, UDP fiable (4 canaux), RPC, lobby et découverte LAN, HTTP(S), `NkNetWorld` (instantanés delta, entrées séquencées, interpolation) | SandboxNKNetwork (67 vérifications, boucle locale), NkNetWorldDemo ; Pong l'utilise | 🔴 ; **ni prédiction, ni rollback, ni lockstep, ni client WebSocket navigateur** |
| **NKSerialization** | `Kernel/System/NKSerialization` (≈ 24 k lignes) | JSON, XML, YAML, binaire, NkNative ; `NkArchive` ; format d'asset ; **`NkAssetExtensionFor` existe déjà** (`NkAssetMetadata.h:285` : `nkprefab`, `nkdata`, `nkmap`…) ; `NkSchemaVersioning` partiel | 15 tests smoke + suites de réflexion | ✅ JSON ; 🔴 versionnage, format d'asset |
| **NKECS** | `Kernel/Runtime/NKECS` (≈ 8 k lignes) | archétypes, requêtes, opérations différées, `NkGameplayEventBus` (`Subscribe` / `Emit`), `NkEntitySerialization` par réflexion | tests 42/42 et 29/0 | ✅ requêtes ; 🔴 bus d'événements, réflexion ; **hiérarchie livrée le 30/09** (`Hierarchy/NkHierarchy.h`, `NkParent` descendu de Noge, suite 26/0) ; prefabs : absents de NKECS, livrés dans Unkeny (`NkUnkenyPrefab`) |
| **Entrées** | `NKEvent`, `NKWindow/Platform/*` | manette (102 boutons, 54 axes, vibration, correspondances à la SDL) ; tactile 32 points ; clavier virtuel ; zone sûre | backends manette réels Win32, Xbox, Linux, Android, Web ; **bouchons** macOS, iOS, UWP, HarmonyOS ; gestes déclarés **sans émetteur** ; IME sans composition | 🔴 dans Unkeny |
| **Localisation** | `NKGui/Doc/NkGuiLangues.h` | clé → texte, bascule à chaud, surcharges `.lang` | NKCode, NKUIDesign | 🔴 ; 8 langues figées, ni pluriels ni droite-à-gauche |
| **NKEditorKit** | `Engine/NKEditorKit` (≈ 40 k lignes) | arbre, navigateur de contenu, onglets, composants décrits, inspecteur par NKReflection, sélecteur de fichiers, menu contextuel, modale, champ, raccourcis, thème | `NkFormProbe.cpp`, NKEditorKitTest ; 5 éditeurs le consomment | ✅ pièces utilisées ; ⚠️ **ni éditeur de graphe, ni frise temporelle, ni pile annuler / refaire** |
| **Plateformes** | Jenga (**hors du dépôt**, `…/Projects/Jenga/Jenga/Core/Builders`) | APK / AAB signés, `.hap`, `.ipa`, `.html + .wasm`, `.app` ; msi / zip / deb / AppImage / dmg ; `Tools/nkdeploy.py` | GemCrush construit pour 4 ABI Android, `.hap`, wasm ; Mou et Pong **ont tourné** sur HarmonyOS | 🔴 pour Unkeny : **Windows seulement** prouvé |
| **Kit Jenga 2D** | `Kit/2D` (**non suivi par git**) | NKCanvas et ses dépendances précompilés, 470 en-têtes, construit le 29/09 à 10:26 | Windows Debug et Release seulement | — ; piste pour compiler le C++ d'un jeu sans le dépôt (document 01, U5) |

### 2.5 Les jeux déjà écrits — et ce qu'ils ont réécrit

| jeu | genre | pile | lignes | banc |
|---|---|---|---|---|
| NkDames, NkEchecs, NkLudo | plateau | NKCanvas + NKGui (`NkCanvasGuiApp`) | 2,5 k / 2,6 k / 3,1 k | `--selftest` : 23 / 21 (perft) / 51 |
| Gemcrush | match-3 (30 niveaux, 3 modes) | NKCanvas + NKGui, boucle maison | 6,9 k | `--selftest`, 8 cas ; **seul jeu qui sauve une progression** |
| Pong | arcade, bonus, réseau local | NKCanvas + NKNetwork | 16,5 k | aucun |
| Mou | éducatif, 6 mini-jeux (prototypes) | NKCanvas + NKGui | 5,0 k | aucun ; a tourné sur HarmonyOS |
| Nkoung | puzzle (laser, labyrinthe) | NKCanvas + NKGui | 2,3 k | aucun ; 2 jeux sur 6 existent |
| ConquerorLab / ConquerorProto | stratégie de territoire | NKEditorKit + NKCanvas | 14,4 k / 0,5 k | harnais d'ABI ; IA en greffons (`exemples/ai/IANegamax.cpp`) |
| Songoo | plateau (songo'o) | GL maison | 3,8 k | **ne compile plus** (désactivé, en-tête disparu — déduit) |
| **Physic2D** | bac à sable physique | **Unkeny** | 2,3 k | banc Unkeny + u1–u10, « 22/22 » (`2fcb386a7`) |

**Genres sans aucun jeu dans le dépôt :** plateforme, shoot'em up, RPG vu de
dessus, roguelike, jeu de cartes, visual novel, tower defense, course, combat.

**Briques recopiées d'un jeu à l'autre** (candidates au moteur, chacune au palier
qui en a besoin) : générateur pseudo-aléatoire LCG (12 copies, 6 applis),
gestionnaire de son (4), gestion des écrans / menus / intro (7), `EaseOutCubic`
(9 copies), IA de tour (5 variantes), particules d'effet (2), coquille et
`--backend=` (6 et 3), zone sûre (2 doublons de `NkUnkenyGeometrie`), tactile
et gestes (4), chargement de niveaux (4), modèle de grille (6), animation de
pièce sur un chemin (3, alors que `NkUnkenyChemin` existe).

---

## 3. Par genre — ce qu'il faut, ce qui existe, ce qui manque

Chaque genre reçoit **un petit jeu d'exemple** comme jalon : c'est lui qui prouve
le genre, pas la liste de briques.

| genre | jeu d'exemple (jalon) | il faut au moteur | il faut à l'éditeur | existe | manque | palier |
|---|---|---|---|---|---|---|
| **Plateforme** | **« Gelée »** : héros en corps mou, 3 niveaux | entrées par actions, contrôleur (sol, saut, coyote, tampon de saut), plateformes à sens unique, caméra qui suit et bornée, tuiles et leurs corps statiques, zones (fin, pics), mort / réapparition | poser et régler le héros, peindre les tuiles, Jouer au clavier / à la manette / au doigt | physique, corps mous, zones rigides, `NkActions` (dormant), `NkVuePosee` (dormant), tuiles (coquille) | sources d'entrée, contrôleurs, zones pour corps mous, raycast 2D exact, dessin et corps des tuiles | **U2**, **U3** |
| **RPG vu de dessus** | **« Le village »** : PNJ, marchand, quête, inventaire | tuiles (sol, murs, eau), déplacement 8 directions, A* sur grille, PNJ à comportement, dialogues, inventaire et objets, quêtes, sauvegarde de partie | éditeur de tuiles, tables de données, éditeur de dialogues, placement de PNJ et de zones | tuiles (coquille), animation d'images, son placé | tout le vocabulaire RPG ; navigation 2D ; arbres de comportement | U3, U7, **U8** |
| **Plateau / cartes** | **NkDames sur Unkeny** + un jeu de cartes simple | grille de plateau (carrée, hexagonale), machine de partie (tours, phases), sièges, annuler un coup, IA de recherche générique, paquet / main / pioche, graine reproductible | éditeur de plateau (taille, cases spéciales), aperçu des sièges | `NkSieges`, `NkChemin`, `NkPlanGrille` (dormants), 3 jeux complets hors Unkeny | adoption par les jeux ; IA de recherche ; cartes | **U6** |
| **Puzzle / match-3** | **« Gemmes »** dans l'éditeur | grille, permutation, cascades, animations d'interpolation, particules d'effet, niveaux en données | éditeur de niveaux de grille, objectifs | Gemcrush complet hors Unkeny | grille générique, courbes (NKAnima non branché), particules visuelles 2D | **U6** |
| **Shoot'em up** | **« Essaim »** : 300 projectiles, un boss | réservoirs d'entités, collisions rapides (CCD 2D), motifs de tir en données, particules visuelles, secousse de caméra, ralenti | éditeur de motifs, profileur | corps rigides, couches et masque, zones | CCD 2D, raycast 2D rapide, pools, particules visuelles, secousse | **U9** |
| **Stratégie / gestion** | **« Colonie »** : 50 unités, ressources, bâtiments | sélection au rectangle, champ de flux, évitement, ressources et horloge de jeu, brouillard, grille de construction | multi-sélection, peinture de coûts de navigation | NKCivilization (agents sur grille, ECS), ConquerorLab (hors Unkeny) | navigation de foule, économie, UI de jeu | **U8** |
| **Jeu physique** (lance-pierre, bac à sable) | **« Catapulte »** : tir, structures qui s'écroulent, score à 3 étoiles | joints 2D, polygones dynamiques justes, destruction, aperçu de trajectoire, stabilité des piles, score | tracer des polygones, poser des joints | **la force d'Unkeny** : rigides + corps mous + fluides + tissus + couteau | joints 2D éprouvés, polygones (défaut probable), destruction | **U9** (bac à sable : ✅ Physic2D) |
| **Visual novel** | **« Récit »** : 10 scènes, choix, 2 langues | dialogue en graphe, variables, texte progressif, portraits, retour arrière, sauvegarde en emplacements, localisation, musique | éditeur de dialogues, table de chaînes | NKGui, NkGuiLangues, NKAudio (musique) | tout le reste ; texte riche absent de NKFont / NKGui | **U7** |

Et ce que **tous** demandent, chacun à son palier :

| besoin transversal | palier | appui existant |
|---|---|---|
| **UI de jeu** (HUD, menus, options) | U7 | NKGui + `.nkgui` de NKUIDesign ; `NkPlanEcran` ; ⚠️ pas de navigation manette dans NKGui |
| **Sauvegarde de partie** | U7 | `.nkscene` (à rendre portable), `NkSchemaVersioning` (partiel) |
| **Entrées multiples** (clavier, manette, doigt, plusieurs joueurs) | U2 | `NkActions`, `NkGamepadSystem`, `NkPointerEvent` |
| **Audio** (musique, bus, placé) | U7 | NKAudio complet ; `NkSons2D` n'en prend que la lecture |
| **Export / empaquetage** par plateforme | U5 | Jenga (hors dépôt), `nkdeploy.py` |

---

## 4. Demande R15 — le corps mou comme personnage

> *« Nos corps mous pourraient être des personnages, par exemple, ou des parties
> de personnage, déplaçables à la souris, au clavier, à la manette ou au
> tactile. »* — Rihen

### 4.1 Ce qui existe (mesuré)

| besoin | appui | fichier |
|---|---|---|
| où est le personnage, à quelle vitesse | `CentreCorps`, `VitesseCorps`, `BoiteCorps` | `NkParticules2D.h` |
| le pousser | `AppliquerVitesse(corps, v)`, `Aimant(c, rayon, accélération, dt)`, `Explosion`, `Translater` | idem |
| le tenir | `EpinglerCorps`, `BasculerEpingle` (épingle **dans l'espace**) | idem |
| l'attraper à la souris | `SaisirDebut` / `SaisirVers` / `SaisirFin` | utilisé en jeu par `NkEditeurVue.cpp:130-180` et par Physic2D |
| son caractère | `raideur`, `pression` (ballon), `formeRaideur` (gelée), `plasticite` (blob), `amortissement` : **modifiables à chaud** | `NkCorpsP2D` |
| en faire une entité | `NkScene::CreerCorpsMou`, `AttacherCorpsMou`, `NkCorpsMou2D` (identifiant stable) | `NkUnkenyScene.h` |
| parler d'actions | `NkActions` (axes, trois états, direction normalisée) | `NkUnkenyActions.h` — **zéro appelant** |
| manette, doigt | `NkGamepadSystem`, `NkPointerEvent::fromTouch` | NKEvent — **pas branchés** |

### 4.2 Ce qui manque, et pourquoi c'est bloquant

1. **`AppliquerVitesse` ÉCRASE la vitesse** de toutes les particules libres
   (`NkParticules2D.cpp:1567-1574`). Appelée à chaque pas pour marcher, elle
   **annule la gravité et les chocs** : le héros flotte. Il faut une
   **impulsion / accélération ajoutée** (et bornée par une vitesse maximale), par
   corps **et** par groupe de particules.
2. **Rien ne dit « je touche le sol ».** Le drapeau `contact` et la `normale` de
   chaque particule sont **remis à faux à la fin de chaque sous-pas**
   (`NkParticules2D.cpp:1137-1159`) : après `Pas()`, un jeu ne peut pas les lire.
   Il faut une requête par corps, tenue sur le dernier sous-pas : nombre de
   contacts, normale moyenne, **sur quoi** (rigide, statique, autre corps mou).
   C'est ce qui permet « sauter seulement au sol ».
3. **Les corps mous n'entrent dans aucune zone.** `NkScene::Relever` ne lit que
   les événements de `NkPhysicsWorld` (`NkUnkenyScene.cpp:479-517`) : une gelée
   qui traverse la zone de fin de niveau, un ramassage ou des pics **ne
   déclenche rien**. Il faut des contacts particules → collisionneur déclencheur,
   traduits en `NkContact2D` avec l'entité du corps mou.
4. **Pas de lien particule ↔ corps rigide.** Une épingle fixe une particule
   **dans le monde**, pas sur un corps. « Des parties de personnage » (un tronc
   rigide et des bras en gelée, une tête en ballon sur un corps en blob) demandent
   une contrainte **particule attachée à un point local d'un `NkRigidBody`**, dans
   les deux sens (la gelée tire le tronc). Lier deux corps mous entre eux
   (`AjouterLien` entre particules de corps différents) n'est **pas vérifié** : les
   liens portent un `corps` et la compaction réordonne les plages.
5. ~~**Pas de hiérarchie d'entités.**~~ **Comblé le 30/09** (branche
   `comble/hierarchie-prefabs`) : `NkScene::Rattacher` / `Detacher`, local et
   monde propagés, Outliner en arbre. ⚠️ Un enfant à corps DYNAMIQUE ou MOU
   n'est pas porté par son parent (la physique le mène) : des parties molles liées
   entre elles passent toujours par les attaches, pas par la hiérarchie.
6. **Aucune entrée n'arrive au jeu.** `NkActions` n'a aucune source ; en
   « Jouer », l'éditeur envoie le clavier à NKGui et la souris aux outils. La
   manette n'est lue par **aucune** application du dépôt hors Sandbox.
7. **La saisie est unique.** `mSaisis` ne tient qu'une saisie à la fois : au
   doigt, deux doigts ne tirent pas deux parties. À évaluer au palier U2.

### 4.3 Ce que U2 construit pour R15

| brique | où | ce qu'elle fait |
|---|---|---|
| `AjouterVitesse`, `AppliquerAcceleration` (corps et groupe) | NKPhysics / `NkParticules2D` | pousser **sans écraser** |
| `ContactsDuCorps` (nombre, normale moyenne, nature de l'autre) | NKPhysics / `NkParticules2D` | le sol, le mur, le plafond |
| attache particule ↔ rigide | NKPhysics / `NkParticules2D` | les parties molles sur un tronc rigide, deux sens |
| zones pour corps mous | Unkeny / `NkScene::Relever` | fin de niveau, ramassage, pics |
| **`NkControleMou2D`** (composant) + son système | Unkeny | actions → marcher (accélération bornée), sauter (impulsion si au sol), se ramasser / s'étaler (`formeRaideur`, `pression`) ; « partie pilotée » = un groupe de particules |
| sources d'entrée | Unkeny / `Entree/` | clavier, manette, doigt (joystick virtuel et boutons **dans la zone sûre**), souris (la saisie existante) |

**Jalon :** « Gelée » (U2), où le héros est une gelée — puis la variante à
parties : un tronc rigide avec deux bras en blob, pilotés séparément.

---

## 5. La feuille de route, palier par palier

### 5.0 Vue d'ensemble

| palier | nom | jalon démontrable |
|---|---|---|
| **U0** | l'état mesuré | Physic2D et l'éditeur : poser, simuler, jouer, sauver une scène de matière |
| **U1** | un éditeur qui ne trahit pas | un niveau de 30 entités construit **à la souris**, gizmos, annuler 20 gestes, projet rouvert **identique** |
| **U2** | jouer vraiment | **« Gelée »** : un jeu de plateforme au héros **mou**, joué au clavier, à la manette et au doigt |
| **U3** | le monde en tuiles | « Gelée » en 3 niveaux **peints** ; la carte du « Village » parcourue |
| **U4** | la logique sans recompiler | une porte en **Blueprint**, un ennemi en **C++ rechargé à chaud**, une pièce en **prefab** |
| **U5** | livrer | « Gelée » en `.exe`, **APK** et page **Web**, depuis l'éditeur |
| **U6** | tours et grilles | **NkDames sur Unkeny**, un jeu de **cartes**, un **match-3** construit dans l'éditeur |
| **U7** | récit et interface | un **visual novel** court (choix, 3 sauvegardes, 2 langues) ; « Gelée » reçoit menus et HUD |
| **U8** | le monde vivant | le **RPG** « Village » (PNJ, dialogue, quête, inventaire) ; la **stratégie** « Colonie » (50 unités) |
| **U9** | action et physique | un **shoot'em up** (300 projectiles) ; un **jeu de lance-pierre** |
| **U10** | ensemble | « Gelée » à **deux en réseau local** et en **écran partagé** |

Les dépendances, palier par palier (U5 élargit ensuite ses cibles à chaque palier
qui suit) :

| palier | a besoin de |
|---|---|
| U1 | U0 |
| U2 | U1 |
| U3 | U1, U2 |
| U4 | U2, document 01 |
| U5 | U1, U2 |
| U6 | U1 (U4 si les règles s'écrivent en Blueprint) |
| U7 | U4, U5 |
| U8 | U3, U4, U7 |
| U9 | U2, U4, U5 |
| U10 | U2, U6, U8 |

📌 **Parallélisme.** U4 (logique) et U5 (livrer) ne dépendent pas l'un de l'autre :
ils peuvent avancer ensemble après U2. U6 n'a pas besoin de la physique : il peut
démarrer dès U1 si l'on accepte d'écrire ses règles en C++ compilé en attendant U4.

📌 **Nommage des témoins.** Les lettres ci-dessous prolongent celles du banc
d'Unkeny (`t` textures, `s` sauvegarde, `a` audio, `j` jeu) et de l'éditeur (`e`).
Elles sont **indicatives** : chaque banc les **pré-enregistre** avant le premier
chiffre, comme les bancs existants, et chaque témoin a sa contre-épreuve (`n`).

---

### U0 — l'état mesuré (29/09/2026)

Tout le § 2. En une ligne : **simulation et sauvegarde ✅, jeu 🔴.**

---

### U1 — un éditeur qui ne trahit pas

**Objectif démontrable.** Construire **à la souris**, sans une ligne de code, un
niveau de blocage de 30 entités (sol, plateformes, caisses, un blob) avec les
gizmos et l'accrochage ; annuler 20 gestes puis les refaire ; l'enregistrer dans
un **projet** ; fermer, déplacer le dossier, rouvrir : **même empreinte**.

**Briques moteur.**
- `NkVue2D::CadrerBoite` (centre **et** zoom pour qu'une boîte tienne) — c'est ce
  qui répond à R13 « même hors cadre » quand la sélection dépasse la vue.
- Rien d'autre : U1 est un palier d'éditeur.

**Briques éditeur.**
- **Finir les demandes R6 à R13** (en cours chez l'autre agent) : molette pressée
  = panoramique ; bouton droit **réservé** au menu contextuel ; clic gauche =
  sélection ; glisser = déplacer ; gizmos W / E / R (Q = sélection), accrochage ;
  **repère d'axes en bas à gauche** (R11, rien d'écrit) ; axes et origine du monde ;
  F = cadrer.
- **Annuler / refaire.** Chaque action de `NkEditeurActions.h` devient une
  **commande** (faire / défaire), la seule porte de l'interface, du clavier et, plus
  tard, des scripts. ⚠️ La voie paresseuse — une photo de scène par geste — est
  **piégée** : `Restaurer` **change les identifiants d'entité**, donc la sélection
  et toute référence tenue par un composant. À n'utiliser que pour les gestes sur
  la matière (couteau, gomme), en le disant.
- **Multi-sélection** (rectangle, Maj / Ctrl), déplacée en gardant les écarts.
- **Projet** : un dossier + un fichier à sa racine, **chemins relatifs**, import
  **copié** dans le projet avec l'origine mémorisée (`CONVENTIONS_FICHIERS.md` § 5).
  ⚠️ L'extension du fichier de projet **n'est pas choisie ici** : « aucun nom n'est
  gravé avant le premier octet écrit » (même document, § 1). `.nkproj` est le nom
  générique d'`ARCHITECTURE.md` ; NK3DModeler a pris le sien (`.nk3dm`) par la
  règle « identifiable sans ouvrir ».
- « Enregistrer sous », choix de fichier (`NkFilePicker` du kit) ; tiroir **Contenu**
  sur les fichiers du projet (`NkContentBrowserModel`, déjà utilisé) ; import
  d'image → texture.
- Essayer enfin **déplacer / agrandir / redimensionner** la fenêtre sans cadre
  (R4, non essayé au commit) ; `outils/pilote_ui.ps1` existe pour cela (« rangé,
  non lancé »).

**Bancs et témoins (éditeur, `--selftest`).**
- (e19) annuler N gestes rend l'empreinte de départ, refaire rend celle
  d'arrivée ; (e19n) un geste non annulable le **dit** au lieu de se taire.
- (e20) gizmo de rotation : l'angle posé égale l'angle du pointeur, conversion par
  `NkVue2D` seulement ; (e20n) avec un zoom et une rotation de vue non nuls.
- (e21) échelle : une échelle nulle ou négative est refusée ou gérée — décision
  écrite avant le témoin.
- (e22) F sur une sélection hors champ **et** plus grande que la vue : elle tient
  entière ; (e22n) sans sélection, la vue ne bouge pas (déjà (e14)).
- (e23) projet déplacé de dossier puis rouvert : même empreinte.
- (e24) multi-sélection déplacée : écarts conservés à 1e-5 près.

**Dépendances.** U0. **Risques.** Le chantier concurrent sur les mêmes fichiers ;
la conversion écran ↔ monde des gizmos (ce dépôt a déjà perdu une journée sur des
pixels de fenêtre pris pour des pixels de viseur, `NkUnkenyCamera.h`).

---

### U2 — jouer vraiment : entrées, caméra, personnages

**Objectif démontrable.** **« Gelée »** : un niveau fait dans l'éditeur (boîtes et
capsules statiques, une plateforme cinématique, des pics, une zone de fin). Le
héros est une **gelée** : il marche, saute **seulement au sol**, s'écrase, atteint
la fin. On le joue **dans l'éditeur** (Jouer) au **clavier**, à la **manette**
(Windows) et **au doigt** (profil téléphone : joystick virtuel dans la zone sûre).
La caméra le suit sans à-coups et ne montre jamais le vide. Variante : un héros
**à parties** (tronc rigide, bras en blob). Variante témoin : le même niveau avec
un héros rigide, pour comparer.

**Briques moteur.**
- **Sources d'entrée → `NkActions`** (la brique existe, elle n'a pas de source) :
  clavier, manette (`NkGamepadSystem`), doigt (joystick et boutons virtuels ancrés
  sur la zone sûre, `NkCibleMin` existe déjà), souris ; table de correspondances
  **en données** ; une table d'actions **par joueur** (prépare U10).
- **Caméra de jeu** : un composant qui **branche `NkVuePosee`** (cible = entité,
  souplesse, bornes) au lieu d'en réécrire une ; zone morte, anticipation.
- **Contrôleur de plateforme rigide** : sol par requête, pente, coyote, tampon de
  saut, saut variable ; **plateformes à sens unique** (la nature existe côté
  tuiles, `NK_PLATEFORME`, rien côté corps).
- **Raycast 2D exact** (NKCollision) : formes exactes et parcours de l'arbre —
  aujourd'hui exact pour le cercle seulement et linéaire (`NkCollisionWorld.cpp:462`).
- **R15** : les six briques du § 4.3.

**Briques éditeur.**
- Jouer **alimente les actions** ; Arrêter les relâche (`ToutRelacher`).
- Panneau **Entrées** (la table de correspondances), et surimpression des actions
  en jeu (débogage).
- « + Ajouter » : Contrôleur de plateforme, Caméra de jeu, Contrôle mou.
- Le profil tactile simule le doigt à la souris (le joystick virtuel se voit et se
  manipule dans le viseur).
- « Jouer depuis ici » (le héros apparaît au point du clic droit).

**Bancs et témoins (moteur, sans fenêtre).**
- (k1) une action posée par programme passe par ses trois états ; (k2) clavier
  et manette **simulés** donnent la même action ; (k3) diagonale normalisée.
- (m1) marcher : la vitesse horizontale visée est atteinte **et la gravité n'est
  pas annulée** ; (m1n) avec l'actuel `AppliquerVitesse`, elle l'est
  (contre-épreuve qui justifie la brique).
- (m2) sauter au sol donne une impulsion ; (m2n) en l'air, refusé.
- (m3) la gelée traverse la zone de fin : un **DÉBUT**, une fois ; (m3n) à côté de
  la zone : rien.
- (m4) bras mou attaché à un tronc rigide : lien tenu à 5 % près, quantité de
  mouvement conservée sur un choc ; (m4n) sans l'attache, le bras tombe.
- (m5) une partie **entière jouée par un script d'actions**, sans fenêtre, atteint
  la fin en moins de N secondes (N pré-enregistré) — c'est la promesse
  « testable » de `NkUnkenyActions.h` enfin tenue.
- (m6) Photographier / Restaurer garde le contrôle et l'état du saut.
- (v1) caméra : même position à 30 et à 120 images par seconde (à 1e-3 près) ;
  (v2) bornes : jamais de vide visible au bord.
- (e25) éditeur : Jouer → actions reçues ; Arrêter → actions relâchées.

**Dépendances.** U1 (composants, gestes). **Risques.** NKPhysics 3D dans l'âme
(`enable2D` non lu) ; saisie unique ; gestes tactiles sans émetteur ; manette
**bouchonnée** sur macOS, iOS, HarmonyOS — « Gelée » à la manette ne se prouve
que sous Windows, Linux, Android et Web.

---

### U3 — le monde en tuiles

**Objectif démontrable.** « Gelée » en **3 niveaux peints** dans l'éditeur
(couches fond / décor / collision, parallaxe, pics et eau par nature de tuile) ;
et la carte du **« Village »** (60 × 40) parcourue par un personnage rigide en
8 directions, collisions de murs comprises.

**Briques moteur.**
- **Dessin de la carte** : hors-champ par zone visible, parallaxe, teinte, atlas
  découpé en cases ; compteurs comme `NkStatsRendu`.
- **`ConstruireCorpsStatiques`** (annoncée, jamais écrite) : bandes de tuiles
  pleines → un corps statique par bande ; mieux, une **chaîne concave** de
  NKCollision pour les contours.
- Nature des tuiles → **événements** (mortelle, eau) ; plateformes à sens unique.
- La carte devient un **composant** d'entité plutôt qu'un membre posé à côté de la
  scène (l'éditeur l'a déjà mise à côté, `NkEditeurModele::carte`) ; plusieurs
  cartes par scène.
- **Sauvegarde** de la carte dans `.nkscene` (**version 2** du format ; la lecture
  accepte toute version ≤, `NK_UNKENY_SCENE_VERSION`).
- Tuiles animées ; **auto-tuilage** (règles de bord) — `stb_herringbone_wang_tile.h`
  dort dans `Externals/Temp`, non intégré.
- À trancher : import **Tiled** (`.tmx`) ou **LDtk** ; grilles isométrique et
  hexagonale (utiles à U6 et U8).

**Briques éditeur.** Mode **Tuiles** : pinceau, rectangle, remplissage, pipette,
gomme ; palette d'atlas ; couches (visibilité, verrou, parallaxe) ; nature des
tuiles de l'atlas ; tout annulable (U1).

**Bancs et témoins.**
- (g1) poser / lire une tuile ; conversions case ↔ monde **aux quatre bords**
  (le piège de l'axe Y inversé, `NkUnkenyTuiles.h`).
- (g2) une rangée de 10 tuiles pleines = 1 corps ; (g2n) un trou = 2 corps.
- (g3) plateforme à sens unique : traversée par dessous, portée par dessus.
- (g4) carte sauvée puis relue : même empreinte ; (g4n) un fichier de version 1
  se relit toujours.
- (g5) hors-champ : une carte de 200 × 200 ne dessine que le visible (compteur) ;
  (g6) temps de dessin mesuré et **écrit**, sans seuil inventé d'avance.

**Dépendances.** U1, U2 (contrôleur, zones). **Risques.** La carte « avant » en
membre du modèle devra migrer ; les anciennes scènes restent lisibles.

---

### U4 — la logique sans recompiler : scripts, prefabs, hiérarchie

**Objectif démontrable.** Dans « Gelée » : une **porte** qui s'ouvre quand la
gelée pèse sur un interrupteur, écrite en **Blueprint** sans C++ et déboguée par
point d'arrêt ; un **ennemi patrouilleur** écrit en **C++** et modifié **pendant
que le jeu tourne** (rechargé, état conservé) ; une **pièce** faite **prefab**,
posée 20 fois, modifiée une fois.

**Briques moteur.**
- **Selon le document 01** : Blueprint sur **NKGraph** (substrat unique,
  sans interpréteur aujourd'hui) ; C++ à chaud (le mécanisme prouvé vit dans
  **Noge**, Windows seulement : l'extraire ou le refaire est la question du 01).
- **Événements de jeu** exposés aux deux langages : contacts, zones (rigides
  **et** corps mous, U2), actions, minuteries, événement d'image d'animation
  (`imageEvent`) — `NkGameplayEventBus` de NKECS est un candidat à évaluer.
- ✅ (30/09) **Hiérarchie** parent / enfant (transform local et monde) —
  NKECS porte la topologie (`NkParent`), Unkeny les transformations.
- ✅ (30/09) **Prefabs** `.nkprefab` : instance, surcharges VUES champ par champ,
  propagation. Les prefabs de Noge posent enfin leurs composants.
- ✅ (30/09) **Composants du jeu écrits champ par champ** — par une description
  explicite (`NkUnkenyChamps.h`), pas par NKReflection : son pont perd `NkVec2f`
  et les tableaux fixes et ne sait rien des références (raisons dans l'en-tête).
  Reste : décrire `NkAnimateur2D` (encore en octets).

**Briques éditeur.** Éditeur de graphes (le canevas de NKGraph n'existe pas, et
NKEditorKit n'a pas d'éditeur de nœuds) ; débogueur (point d'arrêt, valeurs) ;
Outliner **hiérarchique** (glisser un enfant) ✅ ; créer un prefab depuis la
sélection ✅, instancier, surcharger (moteur ✅, pas encore d'interface pour
instancier ni voir les surcharges) ; Détails **génériques** par réflexion pour les
composants du jeu (`NkEditorInspector` du kit s'appuie déjà sur NKReflection).

**Bancs et témoins.** Ceux du document 01 ; plus
- (p1) prefab posé 20 fois, modifié : les 20 suivent, une surcharge est gardée ;
- (h1) un enfant suit son parent en rotation et en échelle ; (h2) détacher garde
  la position monde ;
- (r1) un composant du jeu relu après l'ajout d'un champ : valeur par défaut,
  **aucun décalage** ; (r1n) avec les octets bruts d'aujourd'hui, tout se décale.

**Dépendances.** U2 (événements, zones), document 01. **Risques.** Le C++ à chaud
ne tournera **pas** sur Web ni sur téléphone : le Blueprint y est la seule logique
modifiable sans reconstruire ; NKGraph n'a pas d'exécution.

---

### U5 — livrer : de l'éditeur à l'appareil

**Objectif démontrable.** Depuis l'éditeur, « Gelée » produit un **`.exe`**
Windows, un **APK** Android et une **page Web** ; les trois se lancent et se jouent
(manette sur PC, doigt sur téléphone, clavier sur le Web). Puis `.hap`
(HarmonyOS), Linux, et macOS / iOS quand la signature le permet.

**Briques moteur.**
- **Le joueur autonome** : une application qui charge le projet et lance sa
  première scène — **le même code** que « Jouer » dans l'éditeur (règle 3 du
  document 04 de Nogee : jouer dans l'éditeur, c'est le jeu).
- Cuisson des ressources (textures, sons, scènes) aux extensions de
  `CONVENTIONS_FICHIERS.md` § 2 ; chemins d'assets par plateforme (déjà gérés par
  `NkTextures2D` : `assets/` sur ordinateur, rien sur Android).
- Cycle de vie mobile : pause du son, perte de contexte (`Reteleverser` existe),
  reprise de la simulation sans rattrapage (le plafond de pas existe).
- Profils **Développement / Expédition**.

**Briques éditeur.** Fenêtre **Construire** (plateforme, profil, nom, icône,
orientation), journal de Jenga dans le Journal, « Lancer sur l'appareil »
(`Tools/nkdeploy.py`).

**Bancs et témoins.**
- (l1) le projet cuit, relu par le joueur autonome, a **la même empreinte** que
  dans l'éditeur ;
- (l2) une ressource absente : un message qui la nomme, pas un écran noir ;
- (l3) perte de contexte simulée : textures renvoyées (prolonge (t9)) ;
- construction des trois cibles en intégration continue (GemCrush a déjà une CI
  macOS / iOS / Linux, `7bc27503d`).

**Dépendances.** U1 (projet), U2 (un jeu à livrer) ; U3 et U4 élargissent ce qui
est livré. **Risques.** Jenga vit **hors du dépôt** ; Unkeny n'est prouvé que sous
Windows ; **iOS ne dessine qu'en logiciel** (Metal sans Renderer2D) ; manette
bouchonnée sur macOS / iOS / HarmonyOS ; le Web tourne sans fils (le planificateur
séquentiel d'Unkeny est déjà fait pour ça).

---

### U6 — tours et grilles : plateau, cartes, puzzle

**Objectif démontrable.** **NkDames porté sur Unkeny**, ses 23 vérifications
inchangées ; un **jeu de cartes** simple (pioche, main, défausse, tours, contre
l'IA) ; un **match-3 « Gemmes »** dont les niveaux sont faits dans l'éditeur.

**Briques moteur.**
- **Grille de plateau** en coordonnées **de monde** (carrée, hexagonale ;
  voisinage ; case sous le pointeur **par `NkVue2D`**), là où `NkPlanGrille`
  travaille en pixels d'écran.
- **Machine de partie** : états, tours, phases ; `NkSieges` (enfin appelé) ;
  **annuler un coup**.
- **Règles génériques + recherche** : une interface (coups légaux, jouer, défaire,
  fin, évaluer) et des joueurs IA génériques — aléatoire, glouton, **minimax
  alpha-bêta**, MCTS — qui remplacent les trois `NkNiveauIA` dupliqués (le greffon
  `IANegamax.cpp` de ConquerorLab est un point de départ à lire).
- **Générateur pseudo-aléatoire à graine** (12 copies du même LCG dans le dépôt).
- Cartes : paquet, mélange reproductible, piles, glisser-déposer ;
  **`NkChemin`** pour les coups ; courbes d'interpolation **de NKAnima** (neuf copies
  de `EaseOutCubic` dans le dépôt).

**Briques éditeur.** Éditeur de plateau (taille, forme, cases spéciales) ;
éditeur de niveaux de grille (match-3 : objectifs, coups, gemmes bloquées) ;
aperçu des sièges.

**Bancs et témoins.**
- les bancs des trois jeux (23 / 21 / 51) **rejoués sur la version Unkeny** : mêmes
  résultats ;
- (b1) alpha-bêta profondeur 2 bat le glouton sur N parties (taux
  pré-enregistré) ; (b2) même graine → même partie ; (b3) annuler un coup rend
  l'état exact (empreinte).

**Dépendances.** U1 ; U4 si les règles doivent s'écrire en Blueprint. **Risques.**
Le portage d'un jeu qui marche coûte plus qu'un jeu neuf ; c'est pourtant la
seule preuve que les briques « mesurées sur les trois jeux » les servent.

---

### U7 — récit et interface : visual novel, UI de jeu, sauvegarde, langues

**Objectif démontrable.** **« Récit »**, un visual novel de 10 scènes : portraits,
choix qui embranchent, variables, retour arrière, **3 emplacements de
sauvegarde**, **2 langues** basculées à chaud. Et « Gelée » reçoit un écran titre,
une pause, des options (volumes, touches) et un HUD.

**Briques moteur.**
- **UI de jeu = NKGui** (NKUI est déprécié) + documents `.nkgui` faits dans
  **NKUIDesign** (on ne refait pas l'éditeur d'UI) ; HUD ancré sur la zone sûre.
  ⚠️ NKGui n'a **pas de navigation au focus** : des menus jouables à la manette
  demandent de l'écrire (brique de NKGui, pas d'Unkeny).
- **Dialogue en graphe** (NKGraph), variables, conditions.
- **Texte progressif et texte riche** (couleur, pause, vitesse) — absents de
  NKFont et de NKGui ; bidi et mise en forme complexe restent hors de portée.
- **Sauvegarde de partie** : emplacements, écriture atomique, versionnage
  (`NkSchemaVersioning`, partiel), composants par réflexion (U4), miniature par
  **`NkMiniature`** (enfin appelée).
- **Localisation** : `NkGuiLangues` branchée (8 langues fixes, dont le ghɔmáláʼ ;
  ni pluriels ni droite-à-gauche).
- **Musique et bus** : fondu enchaîné, bus Music / SFX / Voice de NKAudio — que
  `NkSons2D` n'utilise pas encore.

**Briques éditeur.** Éditeur de dialogues ; table de chaînes (clé → texte par
langue) ; aperçu de l'UI au profil d'appareil dans le viseur ; ouverture d'un
`.nkgui` dans NKUIDesign.

**Bancs et témoins.**
- (d1) le choix B mène au nœud attendu, variables comprises ; (d2) retour arrière
  exact.
- (w1) sauver → recharger : même état de partie ; (w1n) une sauvegarde d'une
  version antérieure du jeu se relit (migration) ; (w2) coupure simulée pendant
  l'écriture : l'ancien fichier est intact.
- (i1) bascule de langue : aucun texte ne reste dans l'ancienne ; clé manquante
  **signalée**.
- (u1) le HUD tient dans la zone sûre des 6 profils d'appareil.

**Dépendances.** U4 (graphe, réflexion), U5 (chemins de sauvegarde sur appareil).

---

### U8 — le monde vivant : RPG, stratégie, IA

**Objectif démontrable.** **« Village »** (RPG vu de dessus) : des PNJ patrouillent,
voient le joueur, fuient ou le suivent ; un marchand parle (U7) et vend ; une
quête « ramène 3 champignons » ; un inventaire. **« Colonie »** (stratégie) :
50 unités sélectionnées au rectangle vont au clic sans se chevaucher ; ressources,
un bâtiment, une horloge de jeu.

**Briques moteur.**
- **Navigation 2D** : A* sur grille et tuiles (coûts, diagonales sans couper les
  coins), lissage par ligne de vue, **champ de flux** pour les foules, évitement
  et pilotage (séparation, arrivée). Le navmesh de NKNavigation (plan XZ) est à
  adapter, pas à recopier.
- **Arbres de comportement + blackboard** (absents du dépôt), édités sur NKGraph
  (« on unifie l'autorat, jamais l'exécution »).
- **Perception 2D** : cône de vue par raycast 2D (U2), ouïe par les sons placés.
- **Données de jeu** en tables `.nkdata` : objets, inventaire, quêtes, stats.
- **IA apprenante, en option** — pour « IA et bien plus » : un environnement NKRL
  **sur une scène Unkeny sans fenêtre**, piloté par `NkActions` (c'est exactement
  ce que les actions rendent possible) ; NKEvolve pour régler un contrôleur ;
  NKCivilization pour une simulation d'agents de gestion. ⚠️ NKRL, NKAgent,
  NKCivilization tirent NKTensor → NKRHI / NKSL : mesurer ce que cela coûte à un
  jeu Web ou mobile **avant** de les lier au joueur ; les garder côté entraînement
  (éditeur, banc) est la voie sûre.

**Briques éditeur.** Coûts de navigation peints sur les tuiles ; chemins et champ
de flux visibles ; éditeur d'arbres de comportement ; débogueur d'IA (blackboard en
direct) ; tables de données.

**Bancs et témoins.**
- (n1) A* : chemin optimal sur des grilles de référence ; (n1n) injoignable :
  échec **dit** ; (n2) pas de coin coupé ;
- (n3) 50 unités arrivent sans chevauchement au-delà d'un seuil pré-enregistré ;
- (i2) arbre de comportement : séquence, sélecteur, décorateur sur scénarios
  écrits ;
- (q1) quête : conditions → états → récompense ;
- (x1) facultatif : un agent Q-learning apprend le niveau 1 de « Gelée » (taux de
  réussite pré-enregistré).

**Dépendances.** U3 (tuiles), U4 (logique), U7 (dialogues, UI).

---

### U9 — action et physique : shoot'em up, lance-pierre

**Objectif démontrable.** **« Essaim »** : un shoot'em up vertical, 3 motifs de
tir, 300 projectiles à l'écran, un boss, à 60 images par seconde **sur
téléphone**. **« Catapulte »** : tir à la souris ou au doigt, structures de caisses,
de poutres et de blobs qui s'écroulent, score à 3 étoiles.

**Briques moteur.**
- **Réservoirs d'entités** et destruction différée ; 0 allocation par trame.
- **Collisions rapides** : CCD 2D (aujourd'hui `SweepBody` exclut la 2D) ou
  balayage de projectiles ; raycast 2D par l'arbre.
- 🔶 *(2026-09-30 : `Unkeny/Effets` + `Rendu/NkUnkenyRenduEffets`, six préréglages,
  déterministes, plafonnés ; f6 éprouvé par le banc P5. Reste pour ce palier :
  les réservoirs d'entités et le 0 allocation de f2.)*
- **Particules visuelles 2D** (étincelles, fumée, débris) — NKVFX dépend de
  NKRenderer, il n'y a **aucun émetteur 2D** : à écrire dans `Unkeny/Rendu`, en
  factorisant `ParticleSystem` (Pong) et `MouFx` (Mou).
- Secousse de caméra, ralenti.
- **Joints 2D éprouvés** (distance, pivot, soudure : API 3D, jamais testés en 2D) ;
  **polygones 2D dynamiques** (défaut probable à confirmer, `NkRigidBody.h:129`) ;
  **destruction** (casser un corps) ; aperçu de trajectoire ; stabilité des piles.

**Briques éditeur.** Poser un joint entre deux corps ; tracer un polygone ;
éditeur de motifs de tir ; profileur (temps par système, nombre de corps).

**Bancs et témoins.**
- (f1) un projectile à 200 m/s ne traverse pas un mur de 0,1 m ; (f1n) sans
  balayage, il le traverse ;
- (f2) 300 projectiles : 0 allocation par trame ;
- (f3) une pile de 10 caisses reste debout 10 s ;
- (f4) joint pivot 2D : distance tenue ;
- (f5) polygone dynamique : ses sommets suivent le corps — **confirme ou infirme**
  le défaut lu dans le code ;
- (f6) les particules visuelles ne changent rien à la simulation.

**Dépendances.** U2, U4, U5 (mesure sur téléphone).

---

### U10 — ensemble : multijoueur

**Objectif démontrable.** « Gelée » à **deux en réseau local**, lancé depuis
l'éditeur (deux instances) ; et en **écran partagé** à deux manettes.

**Briques moteur.**
- Écran partagé : `NkVuePosee` × 2 et une table d'actions par joueur (U2).
- **Réplication** d'entités Unkeny sur `NkNetWorld` (instantanés delta, entrées
  séquencées, interpolation) ; ce qui manque : prédiction et réconciliation,
  rollback ou lockstep, client WebSocket **navigateur**, traversée de NAT ; état
  répliqué borné à 256 octets par entité.
- **Lockstep** pour les jeux à tours (U6) : le pas fixe et le planificateur
  séquentiel d'Unkeny le favorisent ; `NkControleur::NK_RESEAU`, prévu dans
  `NkUnkenySieges.h`.
- Lobby (`NkLobby`, découverte LAN — la réception de messages n'est pas câblée).

**Briques éditeur.** « Jouer à 2 », latence et pertes simulées, vue de la
réplication.

**Bancs et témoins.** (r2) deux mondes en boucle locale convergent ; (r2n) 10 %
de pertes : pas de désynchronisation ; (r3) une partie de dames en réseau : même
empreinte des deux côtés.

**Dépendances.** U2, U6, U8.

---

## 6. Règles de conduite de cette feuille de route

1. **Un palier est fini quand son jeu d'exemple se joue** et que ses témoins sont
   verts — pas quand ses en-têtes existent.
2. **Chercher avant d'écrire**, en commençant par la couche du dessous (règle
   d'Unkeny). Les § 2.4 et 2.5 sont ce premier balayage ; il vieillit.
3. **Une brique descend dans Unkeny quand un jeu d'exemple s'en sert**, pas avant ;
   une brique dormante est **branchée ou retirée** par son palier (§ 2.2).
4. **Une brique générique d'éditeur s'écrit dans NKEditorKit** (éditeur de
   graphes, frise, pile de commandes) : NK3DModeler, NkAnimaEditor, Nogee en ont
   besoin aussi.
5. **Jamais NKRenderer dans Unkeny**, ni par un module qui le tire (NKVFX
   aujourd'hui).
6. **Pré-enregistrer chaque témoin** avant son premier chiffre, avec sa
   contre-épreuve — comme les bancs existants.

---

## 7. Non vérifié, et risques

### 7.1 Ce que ce document n'a pas vérifié

- **Rien n'a été compilé ni exécuté.** Les « 37/37 », « 14/14 », « 22/22 »
  viennent des messages de `b1c5e0643` et `2fcb386a7` (Windows 11, clang-mingw,
  Debug).
- **Les documents de règles non suivis par git** (`CLAUDE.md`,
  `GUIDE_GIT_MULTI_AGENTS.md`, `ETAT_TRAVAUX.md`, `PLATEFORMES_ETAT.md`) sont
  **absents** de ce dossier. Leurs blocs de décision — dont « substrat de graphe
  unique NKGraph » et « substrats animation et comportement » — n'ont été lus
  qu'à travers les fichiers qui les citent.
- L'état de l'éditeur **en cours** (R6–R13) est celui de l'arbre de travail au
  moment de la lecture ; il bouge.
- Le défaut du **polygone 2D dynamique** est déduit du code.
- La **casse de Songoo** est déduite d'un en-tête absent.
- Le fonctionnement réel des `NkRenderTexture` hors OpenGL, de NKUIDesign, et
  des bancs de NKGui n'a pas été rejoué.
- La navigation de NkNavDemo (bloquée tant que Noge ne compile pas, d'après son
  en-tête) : état actuel inconnu.
- Lier deux corps mous par `AjouterLien` : non vérifié.

### 7.2 Les risques

| risque | effet | parade | palier |
|---|---|---|---|
| ~~**Composants du jeu en octets bruts** dans `.nkscene`~~ **comblé le 30/09** pour tout composant DÉCRIT (`.nkscene` v2) | reste : un composant non décrit (dont `NkAnimateur2D`) voyage encore en octets | décrire ses champs à `PhotographierAussi` | U4, avant U5 et U7 |
| **`Restaurer` change les POIGNÉES d'entité** — **identité stable livrée le 30/09** | un composant décrit qui tient une entité ne pointe plus faux (`NK_ENTITE`) ; « Arrêter » oublie encore la sélection (témoin (e5)), la retrouver par son uid est une décision à prendre | commandes pour annuler ; `EntiteParUid` | U1, U4 |
| **NKPhysics 3D dans l'âme** | tunnels (pas de CCD 2D), joints 2D inconnus, masse approchée hors cercle et boîte | témoins 2D dédiés avant de s'appuyer dessus | U2, U9 |
| **Briques dormantes** | se lisent comme livrées ; divergent au premier vrai appelant | § 2.2 : brancher ou retirer | U2, U3, U6, U7 |
| **L'IA tire NKTensor → NKRHI / NKSL** | poids et plateformes d'un jeu 2D | garder l'apprentissage côté éditeur et banc ; NKEvolve pour le léger | U8 |
| **C++ à chaud : Windows seulement, dans Noge** | pas de logique modifiable sans reconstruire sur Web et mobile | Blueprint comme logique portable (document 01) | U4 |
| **iOS sans Renderer2D Metal** ; shaders personnalisés sous OpenGL seulement | un jeu à effets ne se ressemble pas partout | ne pas promettre d'effets par shader avant ce chantier de NKCanvas | U5, U9 |
| **Manette bouchonnée** (macOS, iOS, UWP, HarmonyOS) ; gestes tactiles sans émetteur | R15 « à la manette » non tenu partout | écrire les backends manquants dans NKWindow | U2, U5 |
| **NKGui sans navigation au focus** | menus injouables à la manette et à la télécommande | brique de NKGui | U7 |
| **Jenga hors du dépôt** | un clone neuf ne construit pas pour mobile sans lui | le dire dans la fenêtre Construire | U5 |
| **Documentation qui dérive** (README de NKPhysics « scaffold », `Kernel/AI/README.md` « aucun code », en-tête de `NkUnkenySpriteAnim.h`, `NkUnkenyCameraReelle.h` fantôme) | on croit ce qui est faux | corriger au passage, sur les parties qu'on touche (`CONVENTIONS_CODE.md` § 6) | tous |
| **Chantier concurrent** sur UnkenyEditor | ce document décrit un état qui bouge | relire le tableau des demandes à chaque palier | U1 |
